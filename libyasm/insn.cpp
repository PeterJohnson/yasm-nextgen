/*
 * Mnemonic instruction bytecode
 *
 *  Copyright (C) 2005-2007  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "util.h"

#include <algorithm>
#include <iomanip>
#include <boost/bind.hpp>
#include <boost/ref.hpp>

#include "expr.h"
#include "insn.h"


namespace yasm {

#if 0
void
yasm_ea_set_segreg(yasm_effaddr *ea, uintptr_t segreg)
{
    if (!ea)
        return;

    if (segreg != 0 && ea->segreg != 0)
        yasm_warn_set(YASM_WARN_GENERAL,
                      N_("multiple segment overrides, using leftmost"));

    ea->segreg = segreg;
}
#endif
Insn::Operand::Operand(const Register* reg)
    : m_type(REG),
      m_targetmod(0),
      m_size(0),
      m_deref(0),
      m_strict(0)
{
    m_data.reg = reg;
}

Insn::Operand::Operand(const SegmentRegister* segreg)
    : m_type(SEGREG),
      m_targetmod(0),
      m_size(0),
      m_deref(0),
      m_strict(0)
{
    m_data.segreg = segreg;
}

Insn::Operand::Operand(/*@only@*/ EffAddr* ea)
    : m_type(MEMORY),
      m_targetmod(0),
      m_size(0),
      m_deref(0),
      m_strict(0)
{
    m_data.ea = ea;
}

Insn::Operand::Operand(/*@only@*/ Expr* val)
    : m_type(IMM),
      m_targetmod(0),
      m_size(0),
      m_deref(0),
      m_strict(0)
{
    const Register* reg;

    reg = val->get_reg();
    if (reg) {
        m_type = REG;
        m_data.reg = reg;
        delete val;
    } else
        m_data.val = val;
}

Insn::Operand::~Operand()
{
    switch (m_type) {
        //case MEMORY:    delete m_data.ea; break;
        case IMM:       delete m_data.val; break;
        default:        break;
    }
}

void
Insn::Operand::put(std::ostream& os, int indent_level) const
{
    switch (m_type) {
        case REG:
            //os << std::setw(indent_level) << "";
            //os << "Reg=" << *m_data.reg << std::endl;
            break;
        case SEGREG:
            //os << std::setw(indent_level) << "";
            //os << "SegReg=" << *m_data.segreg << std::endl;
            break;
        case MEMORY:
            //os << std::setw(indent_level) << "";
            //os << "Memory=" << *m_data.ea << std::endl;
            break;
        case IMM:
            os << std::setw(indent_level) << "";
            os << "Imm=" << *m_data.val << std::endl;
            break;
    }
    //os << std::setw(indent_level+1) << "";
    //os << "TargetMod=" << *m_targetmod << std::endl;
    os << std::setw(indent_level+1) << "" << "Size=" << m_size << std::endl;
    os << std::setw(indent_level+1) << "";
    os << "Deref=" << m_deref << ", Strict=" << m_strict << std::endl;
}

void
Insn::put(std::ostream& os, int indent_level) const
{
    std::for_each(m_operands.begin(), m_operands.end(),
                  boost::bind(&Insn::Operand::put, _1,
                              boost::ref(os),
                              indent_level));
}

void
Insn::Operand::finalize()
{
#if 0
    yasm_error_class eclass;
    char *str, *xrefstr;
    unsigned long xrefline;
#endif

    switch (m_type) {
#if 0
        case MEMORY:
            /* Don't get over-ambitious here; some archs' memory expr
             * parser are sensitive to the presence of *1, etc, so don't
             * simplify reg*1 identities.
             */
            if (m_data.ea)
                m_data.ea->disp.abs->level_tree(true, true, false, false);
            if (error_occurred()) {
                /* Add a pointer to where it was used to the error */
                error_fetch(eclass, str, xrefline, xrefstr);
                if (xrefstr) {
                    error_set_xref(xrefline, "%s", xrefstr);
                    yasm_xfree(xrefstr);
                }
                if (str) {
                    error_set(eclass, "%s in memory expression", str);
                    yasm_xfree(str);
                }
                return;
            }
            break;
#endif
        case IMM:
            m_data.val->level_tree(true, true, true, false);
#if 0
            if (error_occurred()) {
                /* Add a pointer to where it was used to the error */
                error_fetch(eclass, str, xrefline, xrefstr);
                if (xrefstr) {
                    error_set_xref(xrefline, "%s", xrefstr);
                    yasm_xfree(xrefstr);
                }
                if (str) {
                    error_set(eclass, "%s in immediate expression", str);
                    yasm_xfree(str);
                }
                return;
            }
#endif
            break;
        default:
            break;
    }
}

void
Insn::finalize()
{
    // Simplify the operands' expressions.
    std::for_each(m_operands.begin(), m_operands.end(),
                  boost::mem_fn(&Operand::finalize));
}

} // namespace yasm