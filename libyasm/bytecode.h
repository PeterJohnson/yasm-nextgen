/**
 * \file libyasm/bytecode.h
 * \brief YASM bytecode interface.
 *
 * \license
 *  Copyright (C) 2001-2007  Peter Johnson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  - Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  - Redistributions in binary form must reproduce the above copyright
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
 * \endlicense
 */
#ifndef YASM_BYTECODE_H
#define YASM_BYTECODE_H

#include <vector>
#include <iostream>
#include <memory>
#include <boost/function.hpp>

namespace yasm {

class Linemap;
class Value;
class Bytecode;
class Section;
class Arch;
class Insn;

/** Convert yasm_value to its byte representation.  Usually implemented by
 * object formats to keep track of relocations and verify legal expressions.
 * Must put the value into the least significant bits of the destination,
 * unless shifted into more significant bits by the shift parameter.  The
 * destination bits must be cleared before being set.
 * \param value         value
 * \param buf           buffer for byte representation
 * \param destsize      destination size (in bytes)
 * \param offset        offset (in bytes) of the expr contents from the start
 *                      of the bytecode (needed for relative)
 * \param bc            current bytecode (usually passed into higher-level
 *                      calling function)
 * \param warn          enables standard warnings: zero for none;
 *                      nonzero for overflow/underflow floating point warnings;
 *                      negative for signed integer warnings,
 *                      positive for unsigned integer warnings
 * \return True if an error occurred, false otherwise.
 */
typedef
    boost::function<bool (Value* value, /*@out@*/ unsigned char* buf,
                          unsigned int destsize, unsigned long offset,
                          Bytecode *bc, int warn)>
    OutputValueFunc;

/** Convert a symbol reference to its byte representation.  Usually implemented
 * by object formats and debug formats to keep track of relocations generated
 * by themselves.
 * \param sym           symbol
 * \param bc            current bytecode (usually passed into higher-level
 *                      calling function)
 * \param buf           buffer for byte representation
 * \param destsize      destination size (in bytes)
 * \param valsize       size (in bits)
 * \param warn          enables standard warnings: zero for none;
 *                      nonzero for overflow/underflow floating point warnings;
 *                      negative for signed integer warnings,
 *                      positive for unsigned integer warnings
 * \return True if an error occurred, false otherwise.
 */
typedef
    boost::function<bool (Symbol* sym, Bytecode* bc, unsigned char* buf,
                          unsigned int destsize, unsigned int valsize,
                          int warn)>
    OutputRelocFunc;

/** A data value. */
class Dataval {
public:
    /** Create a new data value from an expression.
     * \param expn  expression
     */
    Dataval(/*@keep@*/ Expr* expn);

    /** Create a new data value from a string.
     * \param contents      string (may contain NULs)
     * \param len           length of string
     */
    Dataval(/*@keep@*/ char* contents, size_t len);

    /** Create a new data value from raw bytes data.
     * \param contents      raw data (may contain NULs)
     * \param len           length
     */
    Dataval(/*@keep@*/ unsigned char *contents, unsigned long len);
};


/** A bytecode. */
class Bytecode {
public:
    /** Add a dependent span for a bytecode.
     * \param bc            bytecode containing span
     * \param id            non-zero identifier for span; may be any
     *                      non-zero value
     *                      if <0, expand is called for any change;
     *                      if >0, expand is only called when exceeds
     *                      threshold
     * \param value         dependent value for bytecode expansion
     * \param neg_thres     negative threshold for long/short decision
     * \param pos_thres     positive threshold for long/short decision
     */
    typedef
        boost::function<void (Bytecode* bc, int id, const Value* value,
                              long neg_thres, long pos_thres)>
        AddSpanFunc;

    /** Bytecode contents (abstract base class).  Any implementation of a
     * specific bytecode must implement a class derived from this one.
     * The bytecode implementation-specific data is stored in #contents.
     */
    class Contents {
    public:
        Contents() {}
        virtual ~Contents() {}

        /** Prints the implementation-specific data (for debugging purposes).
         * Called from Bytecode::put().
         * \param os            output stream
         * \param indent_level  indentation level
         */
        virtual void put(std::ostream& os, int indent_level) const = 0;

        /** Finalizes the bytecode after parsing.
         * Called from Bytecode::finalize().
         * \param bc            bytecode
         * \param prev_bc       bytecode directly preceding bc
         */
        virtual void finalize(Bytecode* bc, Bytecode* prev_bc) = 0;

        /** Calculates the minimum size of a bytecode.
         * Called from Bytecode::calc_len().
         * The base version of this function internal errors when called,
         * so be very careful using the base function in a derived class!
         * This function should simply add to bc->len and not set it
         * directly (it's initialized by Bytecode::calc_len() prior to
         * passing control to this function).
         *
         * \param bc            bytecode
         * \param add_span      function to call to add a span
         * \return False if no error occurred, true if there was an
         *         error recognized (and output) during execution.
         * \note May store to bytecode updated expressions.
         */
        virtual bool calc_len(Bytecode* bc,
                              Bytecode::AddSpanFunc add_span) = 0;

        /** Recalculates the bytecode's length based on an expanded span
         * length.  Called from Bytecode::expand().
         * The base version of this function internal errors when called,
         * so if used from a derived class, make sure that calc_len() never
         * adds a span.
         * This function should simply add to bc->len to increase the length
         * by a delta amount.
         * \param bc            bytecode
         * \param span          span ID (as given to add_span in calc_len)
         * \param old_val       previous span value
         * \param new_val       new span value
         * \param neg_thres     negative threshold for long/short decision
         *                      (returned)
         * \param pos_thres     positive threshold for long/short decision
         *                      (returned)
         * \return 0 if bc no longer dependent on this span's length,
         *         negative if there was an error recognized (and output)
         *         during execution, and positive if bc size may increase
         *         for this span further based on the new negative and
         *         positive thresholds returned.
         * \note May store to bytecode updated expressions.
         */
        virtual int expand(Bytecode* bc, int span,
                           long old_val, long new_val,
                           /*@out@*/ long& neg_thres,
                           /*@out@*/ long& pos_thres) = 0;

        /** Convert a bytecode into its byte representation.
         * Called from Bytecode::to_bytes().
         * The base version of this function internal errors when called,
         * so be very careful using the base function in a derived class!
         * \param bc            bytecode
         * \param buf           byte representation destination buffer;
         *                      should be incremented as it's written to,
         *                      so that on return its delta from the
         *                      passed-in buf matches the bytecode length
         *                      (it's okay not to do this if an error
         *                      indication is returned)
         * \param output_value  function to call to convert values into
         *                      their byte representation
         * \param output_reloc  function to call to output relocation entries
         *                      for a single sym
         * \return True on error, false on success.
         * \note May result in non-reversible changes to the bytecode, but
         *       it's preferable if calling this function twice would result
         *       in the same output.
         */
        virtual bool to_bytes(Bytecode* bc, unsigned char* &buf,
                              OutputValueFunc output_value,
                              OutputRelocFunc output_reloc = 0) = 0;

        /** Special bytecode classifications.  Most bytecode types should
         * simply not override the get_special() function (which returns
         * #SPECIAL_NONE).  Other return values cause special handling to
         * kick in in various parts of yasm.
         */
        enum SpecialType {
            /** No special handling. */
            SPECIAL_NONE = 0,

            /** Bytecode reserves space instead of outputting data. */
            SPECIAL_RESERVE,

            /** Adjusts offset instead of calculating len. */
            SPECIAL_OFFSET,

            /** Instruction bytecode. */
            SPECIAL_INSN
        };

        virtual SpecialType get_special() const;
    };

    /** Implementation-specific data. */
    std::auto_ptr<Contents> m_contents;

    /** Pointer to section containing bytecode; NULL if not part of a
     * section.
     */
    /*@dependent@*/ /*@null@*/ Section* m_section;

    /** Number of times bytecode is repeated.
     * NULL=1 (to save space in the common case).
     */
    std::auto_ptr<Expr> m_multiple;

    /** Total length of entire bytecode (not including multiple copies). */
    unsigned long m_len;

    /** Number of copies, integer version. */
    long m_mult_int;

    /** Line number where bytecode was defined. */
    unsigned long m_line;

    /** Offset of bytecode from beginning of its section.
     * 0-based, ~0UL (e.g. all 1 bits) if unknown.
     */
    unsigned long m_offset;

    /** Unique integer index of bytecode.  Used during optimization. */
    unsigned long m_bc_index;

    /** Labels that point to this bytecode (as the
     * bytecode previous to the label).
     */
    std::vector<Symbol*> m_symbols;

    /** Create a bytecode of any specified type.
     * \param contents      type-specific data
     * \param line          virtual line (from yasm_linemap)
     */
    Bytecode(std::auto_ptr<Contents> contents, unsigned long line);

    /** Transform a bytecode of any type into a different type.
     * \param contents      new type-specific data
     */
    void transform(std::auto_ptr<Contents> contents);

    /** Set multiple field of a bytecode.
     * A bytecode can be repeated a number of times when output.  This
     * function sets that multiple.
     * \param e     multiple (kept, do not free)
     */
    void set_multiple(std::auto_ptr<Expr> e);

    /*
     *
     * General bytecode factory functions
     *
     */

    /** Create a bytecode containing data value(s).
     * \param data          vector of data values
     * \param size          storage size (in bytes) for each data value
     * \param append_zero   append a single zero byte after each data value
     *                      (if non-zero)
     * \param arch          architecture (optional); if provided, data items
     *                      are directly simplified to bytes if possible
     * \param line          virtual line (from yasm_linemap)
     * \return Newly allocated bytecode.
     */
    static /*@only@*/ Bytecode* create_data
        (const std::vector<Dataval>& data, unsigned int size, int append_zero,
         /*@null@*/ Arch* arch, unsigned long line);

    /** Create a bytecode containing LEB128-encoded data value(s).
     * \param datahead      list of data values (kept, do not free)
     * \param sign          signedness (True=signed, False=unsigned) of each
     *                      data value
     * \param line          virtual line (from yasm_linemap)
     * \return Newly allocated bytecode.
     */
    static /*@only@*/ Bytecode* create_leb128
        (const std::vector<Dataval>& datahead, bool sign, unsigned long line);

    /** Create a bytecode reserving space.
     * \param numitems      number of reserve "items" (kept, do not free)
     * \param itemsize      reserved size (in bytes) for each item
     * \param line          virtual line (from yasm_linemap)
     * \return Newly allocated bytecode.
     */
    static /*@only@*/ Bytecode* create_reserve
        (/*@only@*/ Expr* numitems, unsigned int itemsize,
         unsigned long line);

    /** Get the number of items and itemsize for a reserve bytecode.  If bc
     * is not a reserve bytecode, returns NULL.
     * \param bc            bytecode
     * \param itemsize      reserved size (in bytes) for each item (returned)
     * \return NULL if bc is not a reserve bytecode, otherwise an expression
     *         for the number of items to reserve.
     */
    /*@null@*/ const Expr* reserve_numitems
        (/*@out@*/ unsigned int& itemsize) const;

    /** Create a bytecode that includes a binary file verbatim.
     * \param filename      path to binary file
     * \param start         starting location in file (in bytes) to read data
     *                      from (kept, do not free); may be NULL to indicate
     *                      0
     * \param maxlen        maximum number of bytes to read from the file
     *                      (kept, do not free); may be NULL to indicate no
     *                      maximum
     * \param linemap       line mapping repository
     * \param line          virtual line (from linemap) for the bytecode
     * \return Newly allocated bytecode.
     */
    static /*@only@*/ Bytecode* create_incbin
        (const std::string& filename, /*@only@*/ /*@null@*/ Expr* start,
         /*@only@*/ /*@null@*/ Expr* maxlen, Linemap* linemap,
         unsigned long line);

    /** Create a bytecode that aligns the following bytecode to a boundary.
     * \param boundary      byte alignment (must be a power of two)
     * \param fill          fill data (if NULL, code_fill or 0 is used)
     * \param maxskip       maximum number of bytes to skip
     * \param code_fill     code fill data (if NULL, 0 is used)
     * \param line          virtual line (from yasm_linemap)
     * \return Newly allocated bytecode.
     * \note The precedence on generated fill is as follows:
     *       - from fill parameter (if not NULL)
     *       - from code_fill parameter (if not NULL)
     *       - 0
     */
    static /*@only@*/ Bytecode* create_align
        (/*@keep@*/ Expr* boundary,
         /*@keep@*/ /*@null@*/ Expr* fill,
         /*@keep@*/ /*@null@*/ Expr* maxskip,
         /*@null@*/ const unsigned char** code_fill,
         unsigned long line);

    /** Create a bytecode that puts the following bytecode at a fixed section
     * offset.
     * \param start         section offset of following bytecode
     * \param line          virtual line (from yasm_linemap)
     * \return Newly allocated bytecode.
     */
    static /*@only@*/ Bytecode *create_org
        (unsigned long start, unsigned long line);

    /** Get the section that contains a particular bytecode.
     * \param bc    bytecode
     * \return Section containing bc (can be NULL if bytecode is not part of a
     *         section).
     */
    /*@dependent@*/ /*@null@*/ Section* get_section() const
    { return m_section; }

    /** Add to the list of symrecs that reference a bytecode.  For symrec use
     * only.
     * \param bc    bytecode
     * \param sym   symbol
     */
    void add_symrec(/*@dependent@*/ Symbol* sym);

    /** Destructor. */
    ~Bytecode() {};

    /** Print a bytecode.  For debugging purposes.
     * \param os            output stream
     * \param indent_level  indentation level
     */
    void put(std::ostream& os, int indent_level) const;

    /** Finalize a bytecode after parsing.
     * \param prev_bc       bytecode directly preceding bc in a sequence of
     *                      bytecodes
     */
    void finalize(Bytecode* prev_bc);

    /** Determine the distance between the starting offsets of two bytecodes.
     * \param precbc1       preceding bytecode to the first bytecode
     * \param precbc2       preceding bytecode to the second bytecode
     * \return Distance in bytes between the two bytecodes (bc2-bc1), or
     *         NULL if the distance was indeterminate.
     * \warning Only valid /after/ optimization.
     */
    static /*@null@*/ /*@only@*/ IntNum* calc_dist
        (const Bytecode* precbc1, const Bytecode* precbc2);

    /** Get the offset of the next bytecode (the next bytecode doesn't have to
     * actually exist).
     * \return Offset of the next bytecode in bytes.
     * \warning Only valid /after/ optimization.
     */
    unsigned long next_offset() const;

    /** Resolve EQUs in a bytecode and calculate its minimum size.
     * Generates dependent bytecode spans for cases where, if the length
     * spanned increases, it could cause the bytecode size to increase.
     * Any bytecode multiple is NOT included in the length or spans
     * generation; this must be handled at a higher level.
     * \param add_span      function to call to add a span
     * \return False if no error occurred, True if there was an error
     *         recognized (and output) during execution.
     * \note May store to bytecode updated expressions and the short length.
     */
    bool calc_len(AddSpanFunc add_span);

    /** Recalculate a bytecode's length based on an expanded span length.
     * \param span          span ID (as given to yasm_bc_add_span_func in
     *                      yasm_bc_calc_len)
     * \param old_val       previous span value
     * \param new_val       new span value
     * \param neg_thres     negative threshold for long/short decision
     *                      (returned)
     * \param pos_thres     positive threshold for long/short decision
     *                      (returned)
     * \return 0 if bc no longer dependent on this span's length, negative if
     *         there was an error recognized (and output) during execution,
     *         and positive if bc size may increase for this span further
     *         based on the new negative and positive thresholds returned.
     * \note May store to bytecode updated expressions and the updated length.
     */
    int expand(int span, long old_val, long new_val,
               /*@out@*/ long& neg_thres, /*@out@*/ long& pos_thres);

    /** Convert a bytecode into its byte representation.
     * \param bc            bytecode
     * \param buf           byte representation destination buffer
     * \param bufsize       size of buf (in bytes) prior to call; size of
     *                      the generated data after call
     * \param gap           if nonzero, indicates the data does not really
     *                      need to exist in the object file; if nonzero,
     *                      contents of buf are undefined [output]
     * \param output_value  function to call to convert values into their
     *                      byte representation
     * \param output_reloc  function to call to output relocation entries
     *                      for a single sym
     * \return Newly allocated buffer that should be used instead of buf for
     *         reading the byte representation, or NULL if buf was big enough
     *         to hold the entire byte representation.
     * \note Calling twice on the same bytecode may \em not produce the same
     *       results on the second call, as calling this function may result
     *       in non-reversible changes to the bytecode.
     */
    /*@null@*/ /*@only@*/ unsigned char* to_bytes
        (unsigned char* buf, unsigned long& bufsize,
         /*@out@*/ bool& gap,
         OutputValueFunc output_value,
         /*@null@*/ OutputRelocFunc output_reloc = 0)
        /*@sets *buf@*/;

    /** Get the bytecode multiple value as an integer.
     * \param multiple      multiple value (output)
     * \param calc_bc_dist  True if distances between bytecodes should be
     *                      calculated, false if error should be returned
     *                      in this case
     * \return True on error (set with error_set()), false on success.
     */
    bool get_multiple(/*@out@*/ long& multiple, bool calc_bc_dist);

    /** Get the bytecode multiple value as an expression.
     * \return Bytecode multiple, NULL if =1.
     */
    const Expr* get_multiple_expr() const
    { return m_multiple.get(); };

    /** Get a #Insn structure from an instruction bytecode (if possible).
     * \param bc            bytecode
     * \return Instruction details if bytecode is an instruction bytecode,
     *         otherwise NULL.
     */
    /*@dependent@*/ /*@null@*/ Insn* get_insn();
};

} // namespace yasm

#endif