/*
 * Copyright 1999,2005 The Apache Software Foundation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <log4cxx/rolling/rollingfileappender.h>
#include <log4cxx/helpers/loglog.h>
#include <log4cxx/helpers/synchronized.h>
#include <log4cxx/rolling/rolloverdescription.h>
#include <log4cxx/helpers/fileoutputstream.h>
#include <log4cxx/helpers/bytebuffer.h>
#include <log4cxx/rolling/fixedwindowrollingpolicy.h>
#include <log4cxx/rolling/manualtriggeringpolicy.h>

using namespace log4cxx;
using namespace log4cxx::rolling;
using namespace log4cxx::helpers;
using namespace log4cxx::spi;

IMPLEMENT_LOG4CXX_OBJECT(RollingFileAppender)


/**
 * Construct a new instance.
 */
RollingFileAppender::RollingFileAppender() {
}

/**
 * Prepare instance of use.
 */
void RollingFileAppender::activateOptions(Pool &p) {
  if (rollingPolicy == NULL) {
    FixedWindowRollingPolicyPtr fwrp(new FixedWindowRollingPolicy());
    fwrp->setFileNamePattern(getFile() + LOG4CXX_STR(".%i"));
    rollingPolicy = fwrp;
  }

  //
  //  if no explicit triggering policy and rolling policy is both.
  //
  if (triggeringPolicy == NULL) {
     TriggeringPolicyPtr trig(rollingPolicy);
     if (trig != NULL) {
         triggeringPolicy = trig;
     }
  }

  if (triggeringPolicy == NULL) {
    triggeringPolicy = new ManualTriggeringPolicy();
  }

  {
     synchronized sync(mutex);
     triggeringPolicy->activateOptions(p);
     rollingPolicy->activateOptions(p);

     try {
      RolloverDescriptionPtr rollover =
        rollingPolicy->initialize(getFile(), getAppend(), p);

      if (rollover != NULL) {
        ActionPtr syncAction(rollover->getSynchronous());

        if (syncAction != NULL) {
          syncAction->execute(p);
        }

        setFile(rollover->getActiveFileName());
        setAppend(rollover->getAppend());
        lastRolloverAsyncAction = rollover->getAsynchronous();

        if (lastRolloverAsyncAction != NULL) {
//          Thread runner = new Thread(lastRolloverAsyncAction);
//          runner.start();
        }
      }

      File activeFile(getFile());

      if (getAppend()) {
        fileLength = activeFile.length(p);
      } else {
        fileLength = 0;
      }

      FileAppender::activateOptions(p);
    } catch (std::exception& ex) {
      LogLog::warn(
         LogString(LOG4CXX_STR("Exception will initializing RollingFileAppender named "))
             + getName());
    }
  }

}

/**
   Implements the usual roll over behaviour.

   <p>If <code>MaxBackupIndex</code> is positive, then files
   {<code>File.1</code>, ..., <code>File.MaxBackupIndex -1</code>}
   are renamed to {<code>File.2</code>, ...,
   <code>File.MaxBackupIndex</code>}. Moreover, <code>File</code> is
   renamed <code>File.1</code> and closed. A new <code>File</code> is
   created to receive further log output.

   <p>If <code>MaxBackupIndex</code> is equal to zero, then the
   <code>File</code> is truncated with no backup files created.

 * @return true if rollover performed.
 */
bool RollingFileAppender::rollover(Pool& p) {
  //
  //   can't roll without a policy
  //
  if (rollingPolicy != NULL) {

{
    synchronized sync(mutex);
      //
      //   if a previous async task is still running
      //}
      if (lastRolloverAsyncAction != NULL) {
        //
        //  block until complete
        //
        lastRolloverAsyncAction->close();

        //
        //    or don't block and return to rollover later
        //
        //if (!lastRolloverAsyncAction.isComplete()) return false;
      }

      try {
        RolloverDescriptionPtr rollover(rollingPolicy->rollover(getFile(), p));

        if (rollover != NULL) {
          if (rollover->getActiveFileName() == getFile()) {
            closeWriter();

            bool success = true;

            if (rollover->getSynchronous() != NULL) {
              success = false;

              try {
                success = rollover->getSynchronous()->execute(p);
              } catch (std::exception& ex) {
                LogLog::warn(LOG4CXX_STR("Exception on rollover"));
              }
            }

            if (success) {
              if (rollover->getAppend()) {
                fileLength = File(rollover->getActiveFileName()).length(p);
              } else {
                fileLength = 0;
              }

              if (rollover->getAsynchronous() != NULL) {
                lastRolloverAsyncAction = rollover->getAsynchronous();
//                new Thread(lastRolloverAsyncAction).start();
              }

              setFile(
                rollover->getActiveFileName(), rollover->getAppend(),
                bufferedIO, bufferSize, p);
            } else {
              setFile(
                rollover->getActiveFileName(), true, bufferedIO, bufferSize, p);
            }
          } else {
            OutputStreamPtr os(new FileOutputStream(
                  rollover->getActiveFileName(), rollover->getAppend()));
            WriterPtr newWriter(createWriter(os));
            closeWriter();
            setFile(rollover->getActiveFileName());
            setWriter(newWriter);

            bool success = true;

            if (rollover->getSynchronous() != NULL) {
              success = false;

              try {
                success = rollover->getSynchronous()->execute(p);
              } catch (std::exception& ex) {
                LogLog::warn(LOG4CXX_STR("Exception during rollover"));
              }
            }

            if (success) {
              if (rollover->getAppend()) {
                fileLength = File(rollover->getActiveFileName()).length(p);
              } else {
                fileLength = 0;
              }

              if (rollover->getAsynchronous() != NULL) {
                lastRolloverAsyncAction = rollover->getAsynchronous();
//                new Thread(lastRolloverAsyncAction).start();
              }
            }

            writeHeader(p);
          }

          return true;
        }
      } catch (std::exception& ex) {
        LogLog::warn(LOG4CXX_STR("Exception during rollover"));
      }
    }

  }

  return false;
}

/**
 * {@inheritDoc}
*/
void RollingFileAppender::subAppend(const LoggingEventPtr& event, Pool& p) {
  // The rollover check must precede actual writing. This is the
  // only correct behavior for time driven triggers.
  if (
    triggeringPolicy->isTriggeringEvent(
        this, event, getFile(), getFileLength())) {
    //
    //   wrap rollover request in try block since
    //    rollover may fail in case read access to directory
    //    is not provided.  However appender should still be in good
    //     condition and the append should still happen.
    try {
      rollover(p);
    } catch (std::exception& ex) {
        LogLog::warn(LOG4CXX_STR("Exception during rollover attempt."));
    }
  }
  FileAppender::subAppend(event, p);
}

/**
 * Get rolling policy.
 * @return rolling policy.
 */
RollingPolicyPtr RollingFileAppender::getRollingPolicy() const {
  return rollingPolicy;
}

/**
 * Get triggering policy.
 * @return triggering policy.
 */
TriggeringPolicyPtr RollingFileAppender::getTriggeringPolicy() const {
  return triggeringPolicy;
}

/**
 * Sets the rolling policy.
 * @param policy rolling policy.
 */
void RollingFileAppender::setRollingPolicy(const RollingPolicyPtr& policy) {
  rollingPolicy = policy;
}

/**
 * Set triggering policy.
 * @param policy triggering policy.
 */
void RollingFileAppender::setTriggeringPolicy(const TriggeringPolicyPtr& policy) {
  triggeringPolicy = policy;
}

/**
 * Close appender.  Waits for any asynchronous file compression actions to be completed.
 */
void RollingFileAppender::close() {
  {
  synchronized sync (mutex);
    if (lastRolloverAsyncAction != NULL) {
      lastRolloverAsyncAction->close();
    }
  }

  FileAppender::close();
}

namespace log4cxx {
  namespace rolling {
/**
 * Wrapper for OutputStream that will report all write
 * operations back to this class for file length calculations.
 */
class CountingOutputStream : public OutputStream {
  /**
   * Wrapped output stream.
   */
  private:
  OutputStreamPtr os;

  /**
   * Rolling file appender to inform of stream writes.
   */
  RollingFileAppenderPtr rfa;

  public:
  /**
   * Constructor.
   * @param os output stream to wrap.
   * @param rfa rolling file appender to inform.
   */
  CountingOutputStream(
    OutputStreamPtr& os, RollingFileAppender* rfa) :
      os(os), rfa(rfa) {
  }

  /**
   * {@inheritDoc}
   */
  void close(Pool& p)  {
    os->close(p);
  }

  /**
   * {@inheritDoc}
   */
  void flush(Pool& p)  {
    os->flush(p);
  }

  /**
   * {@inheritDoc}
   */
  void write(ByteBuffer& buf, Pool& p) {
    os->write(buf, p);
    rfa->incrementFileLength(buf.limit());
  }

};
  }
}

/**
   Returns an OutputStreamWriter when passed an OutputStream.  The
   encoding used will depend on the value of the
   <code>encoding</code> property.  If the encoding value is
   specified incorrectly the writer will be opened using the default
   system encoding (an error message will be printed to the loglog.
 @param os output stream, may not be null.
 @return new writer.
 */
WriterPtr RollingFileAppender::createWriter(OutputStreamPtr& os) {
  OutputStreamPtr cos(new CountingOutputStream(os, this));
  return FileAppender::createWriter(cos);
}

/**
 * Get byte length of current active log file.
 * @return byte length of current active log file.
 */
size_t RollingFileAppender::getFileLength() const {
  return fileLength;
}

/**
 * Increments estimated byte length of current active log file.
 * @param increment additional bytes written to log file.
 */
void RollingFileAppender::incrementFileLength(size_t increment) {
  fileLength += increment;
}
