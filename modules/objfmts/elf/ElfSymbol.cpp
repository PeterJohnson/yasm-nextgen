//
// ELF object format symbol
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
#include "ElfSymbol.h"

#include "llvm/ADT/Twine.h"
#include "YAML/emitter.h"
#include "yasmx/Basic/Diagnostic.h"
#include "yasmx/Bytecode.h"
#include "yasmx/Bytes_util.h"
#include "yasmx/Expr.h"
#include "yasmx/Expr_util.h"
#include "yasmx/InputBuffer.h"
#include "yasmx/Location_util.h"
#include "yasmx/Object.h"
#include "yasmx/StringTable.h"

#include "ElfConfig.h"
#include "ElfSection.h"


using namespace yasm;
using namespace yasm::objfmt;

const char* ElfSymbol::key = "objfmt::elf::ElfSymbol";

ElfSymbol::ElfSymbol(const ElfConfig&           config,
                     const llvm::MemoryBuffer&  in,
                     const ElfSection&          symtab_sect,
                     ElfSymbolIndex             index,
                     Section*                   sections[],
                     Diagnostic&                diags)
    : m_sect(0)
    , m_name_index(0)
    , m_value(0)
    , m_symindex(index)
    , m_in_table(true)
    , m_weak_ref(false)
    , m_weak_refr(false)
{
    InputBuffer inbuf(in);

    ElfSize symsize = symtab_sect.getEntSize();
    inbuf.setPosition(symtab_sect.getFileOffset() + index * symsize);
    if (inbuf.getReadableSize() < symsize)
    {
        diags.Report(SourceLocation(), diag::err_symbol_unreadable);
        return;
    }

    config.setEndian(inbuf);

    m_name_index = ReadU32(inbuf);

    if (config.cls == ELFCLASS32)
    {
        m_value = ReadU32(inbuf);
        m_size = Expr(ReadU32(inbuf));
    }

    unsigned char info = ReadU8(inbuf);
    m_bind = ELF_ST_BIND(info);
    m_type = ELF_ST_TYPE(info);
    m_vis = ELF_ST_VISIBILITY(ReadU8(inbuf));

    m_index = static_cast<ElfSectionIndex>(ReadU16(inbuf));
    if (m_index != SHN_UNDEF && m_index < config.secthead_count)
        m_sect = sections[m_index];

    if (config.cls == ELFCLASS64)
    {
        m_value = ReadU64(inbuf);
        m_size = Expr(ReadU64(inbuf));
    }
}

ElfSymbol::ElfSymbol()
    : m_sect(0)
    , m_name_index(0)
    , m_value(0)
    , m_index(SHN_UNDEF)
    , m_bind(STB_LOCAL)
    , m_type(STT_NOTYPE)
    , m_vis(STV_DEFAULT)
    , m_symindex(STN_UNDEF)
    , m_in_table(true)
    , m_weak_ref(false)
    , m_weak_refr(false)
{
}

ElfSymbol::~ElfSymbol()
{
}

SymbolRef
ElfSymbol::CreateSymbol(Object& object, const StringTable& strtab) const
{
    SymbolRef sym(0);
    std::string name = strtab.getString(m_name_index);

    if (m_bind == STB_GLOBAL || m_bind == STB_WEAK)
    {
        sym = object.getSymbol(name);
        if (m_index == SHN_UNDEF)
            sym->Declare(Symbol::EXTERN);
        else
            sym->Declare(Symbol::GLOBAL);
    }
    else
    {
        // don't index by name, just append
        sym = object.AppendSymbol(name);
    }

    if (m_index == SHN_ABS)
    {
        if (hasSize())
            sym->DefineEqu(m_size);
        else
            sym->DefineEqu(Expr(0));
    }
    else if (m_index == SHN_COMMON)
    {
        sym->Declare(Symbol::COMMON);
    }
    else if (m_sect != 0)
    {
        Location loc = {&m_sect->bytecodes_front(), m_value.getUInt()};
        sym->DefineLabel(loc);
    }

    return sym;
}

void
ElfSymbol::Write(YAML::Emitter& out) const
{
    out << YAML::BeginMap;
    out << YAML::Key << "type" << YAML::Value << key;
    out << YAML::Key << "sect" << YAML::Value;
    if (m_sect)
        out << YAML::Alias("SECT@" + m_sect->getName());
    else
        out << YAML::Null;
    out << YAML::Key << "value" << YAML::Value << m_value;
    out << YAML::Key << "size source"
        << YAML::Value << m_size_source.getRawEncoding();
    out << YAML::Key << "size" << YAML::Value << m_size;
    out << YAML::Key << "index" << YAML::Value << m_index;

    out << YAML::Key << "bind" << YAML::Value;
    switch (m_bind)
    {
        case STB_LOCAL:     out << "local";     break;
        case STB_GLOBAL:    out << "global";    break;
        case STB_WEAK:      out << "weak";      break;
        default:            out << static_cast<int>(m_bind); break;
    }

    out << YAML::Key << "symtype" << YAML::Value;
    switch (m_type)
    {
        case STT_NOTYPE:    out << "notype";    break;
        case STT_OBJECT:    out << "object";    break;
        case STT_FUNC:      out << "func";      break;
        case STT_SECTION:   out << "section";   break;
        case STT_FILE:      out << "file";      break;
        case STT_COMMON:    out << "common";    break;
        case STT_TLS:       out << "tls";       break;
        default:            out << static_cast<int>(m_type); break;
    }

    out << YAML::Key << "vis" << YAML::Value;
    switch (m_vis)
    {
        case STV_DEFAULT:   out << "default";   break;
        case STV_INTERNAL:  out << "internal";  break;
        case STV_HIDDEN:    out << "hidden";    break;
        case STV_PROTECTED: out << "protected"; break;
        default:            out << static_cast<int>(m_vis); break;
    }

    out << YAML::Key << "symindex" << YAML::Value << m_symindex;
    out << YAML::EndMap;
}

void
ElfSymbol::Finalize(Symbol& sym, Diagnostic& diags)
{
    // If symbol is a weakrefr, make it weak at this point.
    if (m_weak_refr)
    {
        if (!sym.isDefined() &&
            (sym.getVisibility() & (Symbol::GLOBAL | Symbol::COMMON)) == 0)
        {
            if (sym.isUsed())
            {
                setInTable(true);
                sym.Declare(Symbol::GLOBAL);
                setBinding(STB_WEAK);
            }
            else
            {
                setInTable(false);
                return;
            }
        }
        else if (!sym.isDefined() &&
                 (sym.getVisibility() & Symbol::GLOBAL) != 0)
        {
            setBinding(STB_GLOBAL);
        }
    }

    // Don't put the LHS of weakrefs into the symbol table unless they're
    // specifically requested.
    if (m_weak_ref && (sym.getVisibility() == Symbol::DLOCAL ||
                       sym.getVisibility() == Symbol::LOCAL))
    {
        setInTable(false);
        return;
    }

    // If symbol is in a TLS section, force its type to TLS.
    Location loc;
    Section* sect;
    ElfSection* elfsect;
    if (sym.getLabel(&loc) &&
        (sect = loc.bc->getContainer()->AsSection()) &&
        (elfsect = sect->getAssocData<ElfSection>()) &&
        (elfsect->getFlags() & SHF_TLS))
    {
        m_type = STT_TLS;
    }

    // get size (if specified); expr overrides stored integer
    if (!m_size.isEmpty())
    {
        if (!ExpandEqu(m_size))
        {
            diags.Report(m_size_source, diag::err_equ_circular_reference);
            return;
        }
        SimplifyCalcDist(m_size, diags);
        if (!m_size.isIntNum())
            diags.Report(m_size_source, diag::err_size_integer);
    }

    // get EQU value for constants
    const Expr* equ_expr_c = sym.getEqu();

    if (equ_expr_c != 0)
    {
        Expr equ_expr = *equ_expr_c;
        if (!ExpandEqu(equ_expr))
        {
            diags.Report(sym.getDefSource(), diag::err_equ_circular_reference);
            return;
        }
        SimplifyCalcDist(equ_expr, diags);

        // trivial case: simple integer
        if (equ_expr.isIntNum())
        {
            m_index = SHN_ABS;
            m_value = equ_expr.getIntNum();
            return;
        }

        // otherwise might contain relocatable value (e.g. symbol alias)
        std::auto_ptr<Expr> equ_expr_p(new Expr);
        equ_expr_p->swap(equ_expr);
        Value val(64, equ_expr_p);
        val.setSource(sym.getDefSource());
        if (!val.Finalize(diags, diag::err_equ_too_complex))
            return;
        if (val.isComplexRelative())
        {
            diags.Report(sym.getDefSource(), diag::err_equ_too_complex);
            return;
        }

        // set section appropriately based on if value is relative
        if (val.isRelative())
        {
            SymbolRef rel = val.getRelative();
            Location loc;
            if (!rel->getLabel(&loc) || !loc.bc)
            {
                // Referencing an undefined label? Don't gen the symbol.
                diags.Report(sym.getDefSource(), diag::warn_equ_undef_ref);
                m_in_table = false;
                return;
            }

            m_sect = loc.bc->getContainer()->AsSection();
            m_value = loc.getOffset();
            m_value_rel = rel;
        }
        else
        {
            m_index = SHN_ABS;
            m_value = 0;
        }

        // add in any remaining absolute portion
        if (Expr* abs = val.getAbs())
        {
            SimplifyCalcDist(*abs, diags);
            if (!abs->isIntNum())
            {
                diags.Report(sym.getDefSource(), diag::err_equ_not_integer);
                return;
            }
            m_value += abs->getIntNum();
        }
    }
}

void
ElfSymbol::Write(Bytes& bytes, const ElfConfig& config, Diagnostic& diags)
{
    // Pull referenced elf symbol information type and size
    if (m_value_rel)
    {
        ElfSymbol* elfrel = m_value_rel->getAssocData<ElfSymbol>();
        if (elfrel)
        {
            if (!hasType() && elfrel->hasType())
                m_type = elfrel->m_type;
            if (!hasSize() && elfrel->hasSize())
            {
                m_size = elfrel->m_size;
                m_size_source = elfrel->m_size_source;
                // just in case, simplify it
                SimplifyCalcDist(m_size, diags);
                if (!m_size.isIntNum())
                    diags.Report(m_size_source, diag::err_size_integer);
            }
        }
    }

    bytes.resize(0);
    config.setEndian(bytes);

    Write32(bytes, m_name_index);

    if (config.cls == ELFCLASS32)
    {
        Write32(bytes, m_value);
        Write32(bytes, hasSize() && m_size.isIntNum() ? m_size.getIntNum() : 0);
    }

    Write8(bytes, ELF_ST_INFO(m_bind, m_type));
    Write8(bytes, ELF_ST_OTHER(m_vis));

    if (m_sect)
    {
        ElfSection* elfsect = m_sect->getAssocData<ElfSection>();
        assert(elfsect != 0);
        Write16(bytes, elfsect->getIndex());
    }
    else
    {
        Write16(bytes, m_index);
    }

    if (config.cls == ELFCLASS64)
    {
        Write64(bytes, m_value);
        Write64(bytes, hasSize() ? m_size.getIntNum() : 0);
    }

    if (config.cls == ELFCLASS32)
        assert(bytes.size() == SYMTAB32_SIZE);
    else if (config.cls == ELFCLASS64)
        assert(bytes.size() == SYMTAB64_SIZE);
}
