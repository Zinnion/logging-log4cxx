/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "logunit.h"

#include <apr_general.h>

extern "C" {
void initialize() {
    apr_initialize();
}

extern const char** testlist;

bool suite_sort(const LogUnit::SuiteList::value_type& lhs, const LogUnit::SuiteList::value_type& rhs) {
    return lhs.first < rhs.first;
}

abts_suite* abts_run_suites(abts_suite* suite) {
    LogUnit::SuiteList sorted(LogUnit::getAllSuites());
    std::sort(sorted.begin(), sorted.end(), suite_sort);
    
    for(LogUnit::SuiteList::const_iterator iter = sorted.begin();
        iter != sorted.end();
        iter++) {
        //
        //   if there is an explicit testlist or if the suite is not by default disabled
        //         pump suite through filter
        if(testlist || !iter->second->isDisabled()) {
            suite = iter->second->run(suite);
        }
    }
    apr_terminate();
    return suite;
}
}

using namespace LogUnit;


TestException::TestException() {}
TestException::TestException(const TestException& src) : std::exception(src) {
}

TestException& TestException::operator=(const TestException& src) {
    std::exception::operator=(src);
    return *this;
}


AssertException::AssertException(std::string message, int line) : msg(message), lineno(line) {}

AssertException::AssertException(bool expected, const char* actualExpr, int line) : msg(actualExpr), lineno(line) {
   if (expected) {
        msg.append(" was expected to be true, was false.");
   } else {
        msg.append(" was expected to be true, was false.");
   }
}

AssertException::AssertException(const AssertException& src) 
    : std::exception(src), 
      msg(src.msg), 
     lineno(src.lineno) {
}

AssertException::~AssertException() throw() {
}

AssertException& AssertException::operator=(const AssertException& src) {
    std::exception::operator=(src);
    msg = src.msg;
    lineno = src.lineno;
    return *this;
}

std::string AssertException::getMessage() const {
    return msg;
}

int AssertException::getLine() const {
    return lineno;
}




TestFixture::TestFixture() : tc(0) {}
TestFixture::~TestFixture() {}
void TestFixture::setCase(abts_case* tc) {
      this->tc = tc;
}
void TestFixture::setUp() {}
void TestFixture::tearDown() {}

void TestFixture::assertEquals(const std::string expected, 
     const std::string actual, 
     const char* expectedExpr,
     const char* actualExpr,
     int lineno) {
    abts_str_equal(tc, expected.c_str(), actual.c_str(), lineno);
    if (expected != actual) {
        throw TestException();
    }
}

#if LOG4CXX_LOGCHAR_IS_WCHAR || LOG4CXX_WCHAR_T_API       
void TestFixture::assertEquals(const std::wstring expected, 
    const std::wstring actual, 
    const char* expectedExpr,
    const char* actualExpr,
    int lineno) {
    if (expected != actual) {
        std::string exp;
        std::string act;
        for(std::wstring::const_iterator iter = expected.begin();
            iter != expected.end();
            iter++) {
            if (*iter <= 0x7F) {
               exp.append(1, (char) *iter);
             } else {
                exp.append(1, '?');
            }
        }
        for(std::wstring::const_iterator iter = actual.begin();
            iter != actual.end();
            iter++) {
            if (*iter <= 0x7F) {
                act.append(1, (char) *iter);
             } else {
                act.append(1, '?');
            }
        }
        abts_str_equal(tc, exp.c_str(), act.c_str(), lineno);
        throw TestException();
    }
}
#endif        
#if LOG4CXX_LOGCHAR_IS_UNICHAR || LOG4CXX_UNICHAR_API || LOG4CXX_CFSTRING_API
void TestFixture::assertEquals(const std::basic_string<UniChar> expected, 
     const std::basic_string<UniChar> actual,      
     const char* expectedExpr,
     const char* actualExpr,
int lineno) {
    if (expected != actual) {
        std::string exp;
        std::string act;
        for(std::basic_string<UniChar>::const_iterator iter = expected.begin();
            iter != expected.end();
            iter++) {
            if (*iter <= 0x7F) {
               exp.append(1, (char) *iter);
             } else {
                exp.append(1, '?');
            }
        }
        for(std::basic_string<UniChar>::const_iterator iter = actual.begin();
            iter != actual.end();
            iter++) {
            if (*iter <= 0x7F) {
                act.append(1, (char) *iter);
             } else {
                act.append(1, '?');
            }
        }
        abts_str_equal(tc, exp.c_str(), act.c_str(), lineno);
        throw TestException();
    }
}
#endif        


void TestFixture::assertEquals(const int expected, const int actual, int lineno) {
    abts_int_equal(tc, expected, actual, lineno);
    if (expected != actual) {
        throw TestException();
    }
}


LogUnit::TestSuite::TestSuite(const char* fname) : filename(fname), disabled(false) {}

void LogUnit::TestSuite::addTest(const char*, test_func func) {
   test_funcs.push_back(func);
}

const char* LogUnit::TestSuite::getName() const {
   return filename;
}

void LogUnit::TestSuite::setDisabled(bool newVal) {
    disabled = newVal;
}

bool LogUnit::TestSuite::isDisabled() const {
    return disabled;
}

abts_suite* TestSuite::run(abts_suite* suite) const {
    suite = abts_add_suite(suite, filename);
    for(TestList::const_iterator iter = test_funcs.begin();
        iter != test_funcs.end();
        iter++) {
        abts_run_test(suite, *iter, NULL);
    }
    return suite;
}


LogUnit::SuiteList& LogUnit::getAllSuites() {
    static LogUnit::SuiteList allSuites;
    return allSuites;
}
