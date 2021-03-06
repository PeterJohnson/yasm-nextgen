///
/// String table
///
///  Copyright (C) 2008  Peter Johnson
///
/// Redistribution and use in source and binary forms, with or without
/// modification, are permitted provided that the following conditions
/// are met:
/// 1. Redistributions of source code must retain the above copyright
///    notice, this list of conditions and the following disclaimer.
/// 2. Redistributions in binary form must reproduce the above copyright
///    notice, this list of conditions and the following disclaimer in the
///    documentation and/or other materials provided with the distribution.
///
/// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
/// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
/// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
/// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
/// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
/// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
/// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
/// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
/// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
/// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
/// POSSIBILITY OF SUCH DAMAGE.

#include "yasmx/StringTable.h"

#include "llvm/Support/raw_ostream.h"


using namespace yasm;

StringTable::StringTable(unsigned long first_index)
    : m_first_index(first_index)
{
    m_storage.push_back('\0');
}

StringTable::~StringTable()
{
}

unsigned long
StringTable::getIndex(llvm::StringRef str)
{
    unsigned long end = m_storage.size();
    m_storage.insert(m_storage.end(), str.begin(), str.end());
    m_storage.push_back('\0');
    return m_first_index+end;
}

llvm::StringRef
StringTable::getString(unsigned long index) const
{
    return &m_storage.at(index-m_first_index);
}

void
StringTable::Write(llvm::raw_ostream& os) const
{
    os.write(&m_storage[0], m_storage.size());
}

void
StringTable::Read(const unsigned char* buf, unsigned long size)
{
    m_storage.clear();
    m_storage.insert(m_storage.end(), buf, buf+size);
}
