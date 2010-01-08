# !/usr/bin/env python
# Diagnostic include file generation
#
#  Copyright (C) 2009  Peter Johnson
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND OTHER CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR OTHER CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
scriptname = "gen_diag.py"

groups = dict()
class Group(object):
    def __init__(self, name, subgroups=None):
        self.name = name
        if subgroups is None:
            self.subgroups = []
        else:
            self.subgroups = subgroups
        self.members = []

def add_group(name, subgroups=None):
    if name in groups:
        print "Warning: duplicate group %s" % name
    groups[name] = Group(name, subgroups)

class Diag(object):
    def __init__(self, name, cls, desc, mapping=None, group=""):
        self.name = name
        self.cls = cls
        self.desc = desc
        self.mapping = mapping
        self.group = group

diags = dict()
def add_diag(name, cls, desc, mapping=None, group=""):
    if name in diags:
        print "Warning: duplicate diag %s" % name
    diags[name] = Diag(name, cls, desc, mapping, group)

def add_warning(name, desc, mapping=None, group=""):
    if group:
        if group not in groups:
            print "Unrecognized warning group %s" % group
        else:
            groups[group].members.append(name)
    add_diag(name, "WARNING", desc, mapping, group)

def add_error(name, desc, mapping=None):
    add_diag(name, "ERROR", desc, mapping)

def add_note(name, desc):
    add_diag(name, "NOTE", desc, mapping="FATAL")

def output_diags(f):
    for name in sorted(diags):
        diag = diags[name]
        print >>f, "DIAG(%s, CLASS_%s, diag::MAP_%s, \"%s\", %s)" % (
            diag.name,
            diag.cls,
            diag.mapping or diag.cls,
            diag.desc.encode("string_escape"),
            diag.group and "\"%s\"" % diag.group or "0")

def output_groups(f):
    # enumerate all groups and set indexes first
    for n, name in enumerate(sorted(groups)):
        groups[name].index = n

    # output arrays
    print >>f, "#ifdef GET_DIAG_ARRAYS"
    for name in sorted(groups):
        group = groups[name]
        if group.members:
            print >>f, "static const short DiagArray%d[] = {" % group.index,
            for member in group.members:
                print >>f, "diag::%s," % member,
            print >>f, "-1 };"
        if group.subgroups:
            print >>f, "static const char DiagSubGroup%d[] = { " \
                % group.index,
            for subgroup in group.subgroups:
                print >>f, "%d, " % groups[subgroup].index,
            print >>f, "-1 };"

    print >>f, "#endif // GET_DIAG_ARRAYS\n"

    # output table
    print >>f, "#ifdef GET_DIAG_TABLE"
    for name in sorted(groups):
        group = groups[name]
        print >>f, "  { \"%s\", %s, %s }," % (
            group.name,
            group.members and ("DiagArray%d" % group.index) or "0",
            group.subgroups and ("DiagSubGroup%d" % group.index) or "0")
    print >>f, "#endif // GET_DIAG_TABLE\n"

#####################################################################
# Groups (for command line -W option)
#####################################################################

add_group("comment")
add_group("unrecognized-char")
add_group("orphan-labels")
add_group("uninit-contents")
add_group("size-override")

#####################################################################
# Diagnostics
#####################################################################

add_note("note_previous_definition", "previous definition is here")
# note_matching - this is used as a continuation of a previous diagnostic,
# e.g. to specify the '(' when we expected a ')'.
add_note("note_matching", "to match this '%0'")

# Generic lex/parse errors
add_error("err_parse_error", "parse error")
add_error("err_expected_ident", "expected identifier")
add_error("err_expected_ident_lparen", "expected identifier or '('")
add_error("err_expected_lparen", "expected '('")
add_error("err_expected_rparen", "expected ')'")
add_error("err_expected_lsquare", "expected '['")
add_error("err_expected_rsquare", "expected ']'")
add_error("err_expected_lparen_after", "expected '(' after '%0'")
add_error("err_expected_lparen_after_id", "expected '(' after %0")
add_error("err_expected_plus", "expected '+'")
add_error("err_expected_comma", "expected ','")
add_error("err_expected_colon", "expected ':'")
add_error("err_expected_colon_after_segreg", "expected ':' after segment register")
add_error("err_expected_operand", "expected operand")
add_error("err_expected_memory_address", "expected memory address")
add_error("err_expected_expression", "expected expression")
add_error("err_expected_expression_after", "expected expression after '%0'")
add_error("err_expected_expression_after_id", "expected expression after %0")
add_error("err_expected_expression_or_string",
          "expression or string expected")
add_error("err_expected_string", "expected string")
add_error("err_expected_integer", "expected integer");
add_error("err_expected_filename", "expected filename");
add_error("err_eol_junk", "junk at end of line")

# Unrecognized errors/warnings
add_error("err_unrecognized_value", "unrecognized value")
add_error("err_unrecognized_register", "unrecognized register name")
add_error("err_unrecognized_instruction", "unrecognized instruction")
add_error("err_unrecognized_directive", "unrecognized directive")
add_warning("warn_unrecognized_directive", "unrecognized directive")
add_warning("warn_unrecognized_ident", "unrecognized identifier")

# GAS warnings
add_warning("warn_scale_without_index",
            "scale factor without an index register")

# Value
add_error("err_too_complex_expression", "expression too complex")
add_error("err_value_not_constant", "value not constant")
add_error("err_too_complex_jump", "jump target expression too complex")
add_error("err_invalid_jump_target", "invalid jump target")

# Expression
add_error("err_data_value_register", "data values cannot have registers")
add_error("err_expr_contains_float",
          "expression must not contain floating point value")

# Effective address
add_error("err_invalid_ea_segment", "invalid segment in effective address")

# Immediate
add_error("err_imm_segment_override",
          "cannot have segment override on immediate")

# Label/instruction expected
add_error("err_expected_insn_after_label",
          "instruction expected after label")
add_error("err_expected_insn_after_times",
          "instruction expected after TIMES expression")
add_error("err_expected_insn_label_after_eol",
          "label or instruction expected at start of line")

# File
add_error("err_file_read", "unable to read file '%0': %1")
add_error("err_file_output_seek", "unable to seek on output file",
          mapping="FATAL")
add_error("err_file_output_position",
          "could not get file position on output file",
          mapping="FATAL")

# Align
add_error("err_align_boundary_not_const", "align boundary must be a constant")
add_error("err_align_fill_not_const", "align fill must be a constant")
add_error("err_align_skip_not_const", "align maximum skip must be a constant")
add_error("err_align_code_not_found",
          "could not find an appropriate code alignment size")
add_error("err_align_invalid_code_size", "invalid code alignment size %0")

# Incbin
add_error("err_incbin_start_too_complex", "start expression too complex")
add_error("err_incbin_start_not_absolute", "start expression not absolute")
add_error("err_incbin_start_not_const", "start expression not constant")
add_error("err_incbin_maxlen_too_complex",
          "maximum length expression too complex")
add_error("err_incbin_maxlen_not_absolute",
          "maximum length expression not absolute")
add_error("err_incbin_maxlen_not_const",
          "maximum length expression not constant")
add_warning("warn_incbin_start_after_eof", "start past end of file")

# LEB128
add_warning("warn_negative_uleb128", "negative value in unsigned LEB128")

# Multiple
add_error("err_multiple_too_complex", "multiple expression too complex")
add_error("err_multiple_not_absolute", "multiple expression not absolute")
add_error("err_multiple_setpos",
          "cannot combine multiples and setting assembly position")
add_error("err_multiple_negative", "multiple cannot be negative")
add_error("err_multiple_unknown", "could not determine multiple")

# ORG
add_error("err_org_overlap", "ORG overlap with already existing data")

# Symbol
add_error("err_symbol_undefined", "undefined symbol '%0' (first use)")
add_note("note_symbol_undefined_once",
         "(Each undefined symbol is reported only once.)")
add_error("err_equ_not_integer", "global EQU value not an integer expression")
add_error("err_common_size_not_integer",
          "COMMON data size not an integer expression")

# Insn
add_error("err_equ_circular_reference_mem",
          "circular reference detected in memory expression")
add_error("err_equ_circular_reference_imm",
          "circular reference detected in immediate expression")
add_error("err_too_many_operands", "too many operands, maximum of %0")
add_warning("warn_prefixes_skipped", "skipping prefixes on this instruction")
add_error("err_bad_num_operands", "invalid number of operands")
add_error("err_bad_operand_size", "invalid operand size")
add_error("err_requires_cpu", "requires CPU%0")
add_warning("warn_insn_with_cpu", "identifier is an instruction with CPU%0")
add_error("err_bad_insn_operands",
          "invalid combination of opcode and operands")
add_error("err_missing_jump_form", "no %0 form of that jump instruction exists")
add_warning("warn_multiple_seg_override",
            "multiple segment overrides, using leftmost")

# x86 Insn
add_warning("warn_indirect_call_no_deref", "indirect call without '*'")
add_warning("warn_skip_prefixes", "skipping prefixes on this instruction")
add_error("err_16addr_64mode", "16-bit addresses not supported in 64-bit mode")
add_error("err_bad_address_size", "unsupported address size")
add_error("err_dest_not_src1_or_src3",
          "one of source operand 1 (%0) or 3 (%1) must match dest operand")
add_warning("warn_insn_in_64mode",
            "identifier is an instruction in 64-bit mode")
add_error("err_insn_invalid_64mode", "instruction not valid in 64-bit mode")
add_error("err_data32_override_64mode",
          "cannot override data size to 32 bits in 64-bit mode")
add_error("err_addr16_override_64mode",
          "cannot override address size to 16 bits in 64-bit mode")
add_warning("warn_prefix_in_64mode", "identifier is a prefix in 64-bit mode")
add_warning("warn_reg_in_xxmode", "identifier is a register in %0-bit mode")
add_warning("warn_seg_ignored_in_xxmode",
            "'%0' segment register ignored in %1-bit mode")
add_warning("warn_multiple_lock_rep",
            "multiple LOCK or REP prefixes, using leftmost")
add_warning("warn_ignore_rex_on_jump", "ignoring REX prefix on jump")
add_warning("warn_illegal_rex_insn",
            "REX prefix not allowed on this instruction, ignoring")
add_warning("warn_rex_overrides_internal", "overriding generated REX prefix")
add_warning("warn_multiple_rex", "multiple REX prefixes, using leftmost")

# EffAddr
add_error("err_invalid_ea", "invalid effective address")

# x86 EffAddr
add_warning("warn_fixed_invalid_disp_size", "invalid displacement size; fixed")
add_error("err_invalid_disp_size",
          "invalid effective address (displacement size)")
add_error("err_64bit_ea_not64mode",
          "invalid effective address (64-bit in non-64-bit mode)")
add_warning("warn_rip_rel_not64mode",
            "RIP-relative directive ignored in non-64-bit mode")
add_error("err_16bit_ea_64mode",
          "16-bit addresses not supported in 64-bit mode")

# Optimizer
add_error("err_optimizer_circular_reference", "circular reference detected")
add_error("err_optimizer_secondary_expansion",
          "secondary expansion of an external/complex value")

# Output
add_error("err_too_many_relocs", "too many relocations in section '%0'")
add_error("err_reloc_too_complex", "relocation too complex")
add_error("err_reloc_invalid_size", "invalid relocation size")
add_error("err_wrt_not_supported", "WRT not supported")
add_error("err_wrt_too_complex", "WRT expression too complex")
add_error("err_wrt_across_sections", "cannot WRT across sections")
add_error("err_common_size_too_complex", "common size too complex")
add_error("err_common_size_negative", "common size cannot be negative")

# Label
add_note("note_duplicate_label_prev", "previous label defined here")
add_warning("warn_orphan_label",
            "label alone on a line without a colon might be in error",
            group="orphan-labels")
add_warning("warn_no_nonlocal", "no preceding non-local label")

# Directive
add_error("err_expected_directive_name", "expected directive name")
add_error("err_invalid_directive_argument", "invalid argument to directive")
add_error("err_directive_no_args", "directive requires an argument")
add_warning("warn_directive_one_arg", "directive only uses first argument")
add_error("err_value_id", "value must be an identifier")
add_error("err_value_integer", "value must be an integer")
add_error("err_value_expression", "value must be an expression")
add_error("err_value_string", "value must be a string")
add_error("err_value_string_or_id",
          "value must be a string or an identifier")
add_error("err_value_power2", "value is not a power of 2")
add_error("err_value_register", "value must be a register")
add_warning("warn_unrecognized_qualifier", "unrecognized qualifier")

# Size/offset
add_error("err_no_size", "no size specified")
add_error("err_size_expression", "size must be an expression")
add_error("err_no_offset", "no offset specified")
add_error("err_offset_expression", "offset must be an expression")

# Section directive
add_warning("warn_section_redef_flags",
            "section flags ignored on section redeclaration")

# Operand size override
add_error("err_register_size_override", "cannot override register size")
add_warning("warn_operand_size_override",
            "overridding operand size from %0-bit to %1-bit",
            group="size-override")
add_warning("warn_operand_size_duplicate",
            "duplicate '%0' operand size override",
            group="size-override")

# Lexer 
add_warning("null_in_string",
            "null character(s) preserved in string literal")
add_warning("null_in_char",
            "null character(s) preserved in character literal")
add_warning("null_in_file", "null character ignored",
            group="unrecognized-char")
add_warning("backslash_newline_space",
            "backslash and newline separated by space")
add_warning("warn_unrecognized_char", "ignoring unrecognized character '%0'",
            group="unrecognized-char")

add_warning("warn_multi_line_eol_comment", "multi-line end-of-line comment",
            group="comment")

add_error("err_unterminated_string", "missing terminating %0 character")

# Literals
add_warning("warn_integer_too_large", "integer constant is too large")
add_warning("warn_integer_too_large_for_signed",
            "integer constant is so large that it is unsigned")
add_warning("warn_unknown_escape", "unknown escape sequence '\\%0'")
add_warning("warn_oct_escape_out_of_range",
            "octal escape sequence out of range")
add_error("err_hex_escape_no_digits",
          "\\x used with no following hex digits")
add_error("err_invalid_decimal_digit",
          "invalid digit '%0' in decimal constant")
add_error("err_invalid_binary_digit",
          "invalid digit '%0' in binary constant")
add_error("err_invalid_octal_digit",
          "invalid digit '%0' in octal constant")
add_error("err_invalid_suffix_integer_constant",
          "invalid suffix '%0' on integer constant")
add_error("err_invalid_suffix_float_constant",
          "invalid suffix '%0' on floating constant")
add_error("err_exponent_has_no_digits", "exponent has no digits")

#####################################################################
# Output generation
#####################################################################

if __name__ == "__main__":
    import sys
    if len(sys.argv) != 3:
        print >>sys.stderr, "Usage: gen_diag.py <DiagnosticGroups.inc>"
        print >>sys.stderr, "    <DiagnosticKinds.inc>"
        sys.exit(2)
    output_groups(file(sys.argv[1], "wt"))
    output_diags(file(sys.argv[2], "wt"))