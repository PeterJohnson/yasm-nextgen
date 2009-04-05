//
// ELF object format relocation
//
//  Copyright (C) 2003-2007  Michael Urman
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
#include "ElfReloc.h"

#include <util.h>

#include "yasmx/Support/errwarn.h"
#include "yasmx/Bytes_util.h"

#include "ElfConfig.h"
#include "ElfMachine.h"
#include "ElfSymbol.h"


namespace yasm
{
namespace objfmt
{
namespace elf
{

ElfReloc::ElfReloc(SymbolRef sym,
                   SymbolRef wrt,
                   const IntNum& addr,
                   size_t valsize)
    : Reloc(addr, sym)
    , m_type(0)         // default to no type
    , m_addend(0)
{
    if (wrt)
    {
        const ElfSpecialSymbol* ssym = get_elf_ssym(*wrt);
        if (!ssym || valsize != ssym->size)
            throw TypeError(N_("elf: invalid WRT"));

        // Force TLS type; this is required by the linker.
        if (ssym->thread_local)
        {
            if (ElfSymbol* esym = get_elf(*sym))
                esym->set_type(STT_TLS);
        }
        m_type = ssym->reloc;
    }

    assert(sym != 0 && "sym is null");
}

ElfReloc::ElfReloc(const ElfConfig& config,
                   const ElfSymtab& symtab,
                   std::istream& is,
                   bool rela)
    : Reloc(0, SymbolRef(0))
{
    Bytes bytes;
    config.setup_endian(bytes);

    if (config.cls == ELFCLASS32)
    {
        bytes.write(is, rela ? RELOC32A_SIZE : RELOC32_SIZE);

        m_addr = read_u32(bytes);

        unsigned long info = read_u32(bytes);
        m_sym = symtab.at(ELF32_R_SYM(info));
        m_type = ELF32_R_TYPE(info);

        if (rela)
            m_addend = read_s32(bytes);
    }
    else if (config.cls == ELFCLASS64)
    {
        bytes.write(is, rela ? RELOC64A_SIZE : RELOC64_SIZE);

        m_addr = read_u64(bytes);

        IntNum info = read_u64(bytes);
        m_sym = symtab.at(ELF64_R_SYM(info));
        m_type = ELF64_R_TYPE(info);

        if (rela)
            m_addend = read_s64(bytes);
    }
}

ElfReloc::~ElfReloc()
{
}

Expr
ElfReloc::get_value() const
{
    Expr e(m_sym);
    if (!m_addend.is_zero())
        e += m_addend;
    return e;
}

void
ElfReloc::handle_addend(IntNum* intn, const ElfConfig& config)
{
    // rela sections put the addend into the relocation, and write 0 in
    // data area.
    if (config.rela)
    {
        m_addend = *intn;
        *intn = 0;
    }
}

void
ElfReloc::write(Bytes& bytes, const ElfConfig& config)
{
    unsigned long r_sym = STN_UNDEF;

    if (ElfSymbol* esym = get_elf(*get_sym()))
        r_sym = esym->get_symindex();

    bytes.resize(0);
    config.setup_endian(bytes);

    if (config.cls == ELFCLASS32)
    {
        write_32(bytes, m_addr);
        write_32(bytes,
                 ELF32_R_INFO(r_sym, static_cast<unsigned char>(m_type)));

        if (config.rela)
            write_32(bytes, m_addend);
    }
    else if (config.cls == ELFCLASS64)
    {
        write_64(bytes, m_addr);
        write_64(bytes,
                 ELF64_R_INFO(r_sym, static_cast<unsigned char>(m_type)));
        if (config.rela)
            write_64(bytes, m_addend);
    }
}

}}} // namespace yasm::objfmt::elf