def opersize64(fields, field, val):
    if val == 64:
        fields["rex"] = 1
        fields["rex.w"] = 1
    else:
        fields["opersize.inner"] = val

