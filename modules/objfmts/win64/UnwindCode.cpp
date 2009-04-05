//
// Win64 structured exception handling unwind code
//
//  Copyright (C) 2007  Peter Johnson
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
#include "UnwindCode.h"

#include <util.h>

#include <yasmx/Support/Compose.h>
#include <yasmx/Support/errwarn.h>
#include <yasmx/BytecodeContainer.h>
#include <yasmx/BytecodeContainer_util.h>
#include <yasmx/BytecodeOutput.h>
#include <yasmx/Bytes.h>
#include <yasmx/Bytes_util.h>
#include <yasmx/Expr.h>
#include <yasmx/Symbol.h>


namespace yasm
{
namespace objfmt
{
namespace win64
{

UnwindCode::~UnwindCode()
{
}

void
UnwindCode::put(marg_ostream& os) const
{
    // TODO
}

void
UnwindCode::finalize(Bytecode& bc)
{
    if (!m_off.finalize())
        throw ValueError(N_("offset expression too complex"));
}

unsigned long
UnwindCode::calc_len(Bytecode& bc, const Bytecode::AddSpanFunc& add_span)
{
    unsigned long len = 0;
    int span = 0;
    long low, high, mask;

    len += 1;   // Code and info

    switch (m_opcode)
    {
        case PUSH_NONVOL:
        case SET_FPREG:
        case PUSH_MACHFRAME:
            // always 1 node
            return len;
        case ALLOC_SMALL:
        case ALLOC_LARGE:
            // Start with smallest, then work our way up as necessary
            m_opcode = ALLOC_SMALL;
            m_info = 0;
            span = 1;
            low = 8;
            high = 128;
            mask = 0x7;
            break;
        case SAVE_NONVOL:
        case SAVE_NONVOL_FAR:
            // Start with smallest, then work our way up as necessary
            m_opcode = SAVE_NONVOL;
            len += 2;                   // Scaled offset
            span = 2;
            low = 0;
            high = 8*64*1024-8;         // 16-bit field, *8 scaling
            mask = 0x7;
            break;
        case SAVE_XMM128:
        case SAVE_XMM128_FAR:
            // Start with smallest, then work our way up as necessary
            m_opcode = SAVE_XMM128;
            len += 2;                   // Scaled offset
            span = 3;
            low = 0;
            high = 16*64*1024-16;       // 16-bit field, *16 scaling
            mask = 0xF;
            break;
        default:
            assert(false && "unrecognied unwind opcode");
            return 0;
    }

    IntNum intn;
    if (m_off.get_intnum(&intn, false))
    {
        long intv = intn.get_int();
        if (intv > high)
        {
            // Expand it ourselves here if we can and we're already larger
            if (expand(bc, len, span, intv, intv, &low, &high))
                add_span(bc, span, m_off, low, high);
        }
        if (intv < low)
            throw ValueError(N_("negative offset not allowed"));
        if ((intv & mask) != 0)
            throw ValueError(String::compose(
                N_("offset of %1 is not a multiple of %2"), intv, mask+1));
    }
    else
        add_span(bc, span, m_off, low, high);
    return len;
}

bool
UnwindCode::expand(Bytecode& bc,
                   unsigned long& len,
                   int span,
                   long old_val,
                   long new_val,
                   /*@out@*/ long* neg_thres,
                   /*@out@*/ long* pos_thres)
{
    if (new_val < 0)
        throw ValueError(N_("negative offset not allowed"));

    if (span == 1)
    {
        // 3 stages: SMALL, LARGE and info=0, LARGE and info=1
        assert((m_opcode != ALLOC_LARGE || m_info != 1) &&
               "expansion on already largest alloc");

        if (m_opcode == ALLOC_SMALL && new_val > 128)
        {
            // Overflowed small size
            m_opcode = ALLOC_LARGE;
            len += 2;
        }
        if (new_val <= 8*64*1024-8)
        {
            // Still can grow one more size
            *pos_thres = 8*64*1024-8;
            return true;
        }
        // We're into the largest size
        m_info = 1;
        len += 2;
    }
    else if (m_opcode == SAVE_NONVOL && span == 2)
    {
        m_opcode = SAVE_NONVOL_FAR;
        len += 2;
    }
    else if (m_opcode == SAVE_XMM128 && span == 3)
    {
        m_opcode = SAVE_XMM128_FAR;
        len += 2;
    }
    return false;
}

void
UnwindCode::output(Bytecode& bc, BytecodeOutput& bc_out)
{
    Bytes& bytes = bc_out.get_scratch();

    // Offset value
    unsigned int size;
    int shift;
    long low, high;
    unsigned long mask;
    switch (m_opcode)
    {
        case PUSH_NONVOL:
        case SET_FPREG:
        case PUSH_MACHFRAME:
            // just 1 node, no offset; write opcode and info and we're done
            write_8(bytes, (m_info << 4) | (m_opcode & 0xF));
            return;
        case ALLOC_SMALL:
            // 1 node, but offset stored in info
            size = 0; low = 8; high = 128; shift = 3; mask = 0x7;
            break;
        case ALLOC_LARGE:
            if (m_info == 0)
            {
                size = 2; low = 136; high = 8*64*1024-8; shift = 3;
            }
            else
            {
                size = 4; low = high = 0; shift = 0;
            }
            mask = 0x7;
            break;
        case SAVE_NONVOL:
            size = 2; low = 0; high = 8*64*1024-8; shift = 3; mask = 0x7;
            break;
        case SAVE_XMM128:
            size = 2; low = 0; high = 16*64*1024-16; shift = 4; mask = 0xF;
            break;
        case SAVE_NONVOL_FAR:
            size = 4; low = high = 0; shift = 0; mask = 0x7;
            break;
        case SAVE_XMM128_FAR:
            size = 4; low = high = 0; shift = 0; mask = 0xF;
            break;
        default:
            assert(false && "unrecognied unwind opcode");
            return;
    }

    // Check for overflow
    IntNum intn;
    if (!m_off.get_intnum(&intn, true))
        throw ValueError(N_("offset expression too complex"));
    if (size != 4 && !intn.in_range(low, high))
    {
        throw ValueError(String::compose(
            N_("offset of %1 bytes, must be between %2 and %3"),
            intn, low, high));
    }

    if ((intn.get_uint() & mask) != 0)
    {
        throw ValueError(String::compose(
            N_("offset of %1 is not a multiple of %2"), intn, mask+1));
    }
    intn >>= shift;

    // Stored value in info instead of extra code space
    if (size == 0)
        m_info = intn.get_uint()-1;

    // Opcode and info
    write_8(bytes, (m_info << 4) | (m_opcode & 0xF));

    bytes << little_endian;
    if (size == 2)
        write_16(bytes, intn);
    else if (size == 4)
        write_32(bytes, intn);
    bc_out.output(bytes);
}

UnwindCode*
UnwindCode::clone() const
{
    UnwindCode* uwcode = new UnwindCode(m_proc, m_loc, m_opcode, m_info);
    uwcode->m_off = m_off;
    return uwcode;
}

void
append_unwind_code(BytecodeContainer& container,
                   std::auto_ptr<UnwindCode> uwcode)
{
    // Offset in prolog
    Bytecode& bc = container.fresh_bytecode();
    bc.append_fixed(1,
                    Expr::Ptr(new Expr(SUB(uwcode->m_loc, uwcode->m_proc))),
                    0);

    switch (uwcode->m_opcode)
    {
        case UnwindCode::PUSH_NONVOL:
        case UnwindCode::SET_FPREG:
        case UnwindCode::PUSH_MACHFRAME:
            // just 1 node, no offset; write opcode and info and we're done
            append_byte(container,
                        (uwcode->m_info << 4) | (uwcode->m_opcode & 0xF));
            return;
        default:
            break;
    }

    bc.set_line(uwcode->m_loc->get_def_line());
    bc.transform(Bytecode::Contents::Ptr(uwcode));
}

}}} // namespace yasm::objfmt::win64