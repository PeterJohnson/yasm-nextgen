def ea16(fields, field, expr):
    raise NotImplementedError

def ea32(fields, field, expr):
    raise NotImplementedError

def ea64(fields, field, expr):
    raise NotImplementedError

def modrm64_reg(fields, field, val):
    # val must be a register operand
    if val > 7:
        fields["rex"] = 1
        fields["rex.r"] = 1
    fields["modrm.reg.inner"] = val & 7

def modrm64_rm(fields, field, val):
    # val must be a register operand
    if val > 7:
        fields["rex"] = 1
        fields["rex.b"] = 1
    fields["modrm.rm.inner"] = val & 7

def sib64_index(fields, field, val):
    # val must be a register operand
    if val > 7:
        fields["rex"] = 1
        fields["rex.x"] = 1
    fields["sib.index.inner"] = val & 7

def sib64_base(fields, field, val):
    # val must be a register operand
    if val > 7:
        fields["rex"] = 1
        fields["rex.b"] = 1
    fields["sib.base.inner"] = val & 7

def opersize64(fields, field, val):
    if val == 64:
        fields["rex"] = 1
        fields["rex.w"] = 1
    else:
        fields["opersize.inner"] = val

def opcodereg64(fields, field, reg):
    # val must be a register operand
    if reg.value > 7:
        fields["rex"] = 1
        fields["rex.b"] = 1
    fields["opcode.reg.inner"] = reg.value & 7

