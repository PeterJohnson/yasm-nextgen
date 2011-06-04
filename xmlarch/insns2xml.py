#!/usr/bin/env python
from funcparserlib.lexer import *
from funcparserlib.parser import *
import sys
import re

def lprint(*args, **kwargs):
    sep = kwargs.pop("sep", ' ')
    end = kwargs.pop("end", '\n')
    file = kwargs.pop("file", sys.stdout)
    file.write(sep.join(args))
    file.write(end)

def tokenize(str):
    'str -> Sequence(Token)'
    specs = [
        ('comment', (r'#.*',)),
        ('newline', (r'[\r\n]+',)),
        ('space', (r'[ \t\r\n]+',)),
        ('name', (r'[a-zA-Z_.][a-zA-Z0-9_.]*',)),
        ('string', (r'"([^"\\\n]*(\\.[^"\\\n]*)*)"',)),
        ('number', (r'[0-9]+',)),
        ('op', (r'[\[\]{}<>().@=:;,^!&|]',)),
    ]
    useless = ['comment', 'newline', 'space']
    t = make_tokenizer(specs)
    return [x for x in t(str) if x.type not in useless]

def make_array(n):
    if n is None:
        return []
    else:
        return [n[0]] + n[1]

def make_object(n):
    return dict(make_array(n))

re_esc = re.compile(r'\\["\\/bfnrt]', re.VERBOSE)
def unescape(s):
    std = {
        '"': '"', '\\': '\\', '/': '/', 'b': '\b', 'f': '\f', 'n': '\n',
        'r': '\r', 't': '\t',
    }
    return re_esc.sub(lambda m: m.group(1), s)

class Ref(object):
    def __init__(self, type, idref):
        self.type = type
        self.idref = idref

    def __repr__(self):
        return 'Ref(%r, %r)' % (self.type, self.idref)

    def to_xml(self):
        if self.type == 'tab': s = 'TableRef'
        elif self.type == 'isa': s = 'IsaRef'
        elif self.type == 'Reg': s = 'RegisterRef'
        else: s = self.type

        return '<%s idref="%s"/>' % (s, self.idref)

class Field(object):
    def __init__(self, idref, value):
        self.idref = idref
        self.value = value

    def __repr__(self):
        return 'Field(%r, %r)' % (self.idref, self.value)

    def to_xml(self):
        if self.value is None or self.value == '@':
            return '<SetField idref="%s"/>' % self.idref
        else:
            return '<SetField idref="%s">%s</SetField>' % (self.idref, self.value)

class Match(object):
    def __init__(self, objs, fields=None):
        self.objs = objs
        if fields is None:
            self.fields = []
        else:
            self.fields = fields

    def __repr__(self):
        return 'Match(%r, %r)' % (self.objs, self.fields)

    def to_xml(self):
        s = []
        for obj in self.objs:
            if hasattr(obj, 'to_xml'):
                s.extend(obj.to_xml().split('\n'))
            else:
                s.append('<Identifier>%s</Identifier>' % obj)
        s.extend(x.to_xml() for x in self.fields)
        return '<Match>\n\t' + '\n\t'.join(s) + '\n</Match>'

def make_expr(op, lhs, rhs=None):
    if op == '!':
        return ('!', lhs)
    elif not rhs:
        return lhs
    elif isinstance(rhs, list):
        x = [op, lhs]
        x.extend(rhs)
        return tuple(x)
    else:
        return (op, lhs, rhs)

# Grammar
tokval = lambda tok: tok.value
mktok = lambda t: lambda v: a(Token(t, v)) >> tokval
n = mktok('name')
op = mktok('op')
op_ = lambda s: skip(op(s))
sometok = lambda t: some(lambda x: x.type == t) >> tokval
sometoks = lambda ts: some(lambda x: x.type in ts).named('id') >> tokval

number = sometok('number') >> (lambda n: int(n))
string = sometok('string') >> (lambda n: unescape(n[1:-1]))
name = sometok('name')
idref = sometoks(['name', 'number'])

ref = name + op_('[') + maybe(idref) + op_(']') >> (lambda n: Ref(*n))

refs = ref + many(op_('|') + ref) >> make_array
setter = (name + op_('=') + refs
          >> (lambda n: Match(n[1], [Field(n[0], '@')])))

field = (name + op_('=') + (string | number | op('@'))
         >> (lambda n: Field(*n)))
fields = op_('{') + field + many(op_(',') + field) + op_('}') >> make_array
obj = ref|string
objs = obj + many(op_('|') + obj) >> make_array
matcher = objs + maybe(op_(':') + fields) >> (lambda n: Match(*n))

# ^foo is an alias for tab[foo]
tabref = op_('^') + idref >> (lambda n: Ref('tab', n))

match = setter | matcher | tabref
matches = op_('{') + match + many(op_(',') + match) + op_('}') >> make_array

@with_forward_decls
def cexprterm():
    return ref | (op_('(') + cexpr + op_(')'))
cexpr1 = cexprterm | (op_('!') + cexprterm >> (lambda n: make_expr('!', n)))
cexpr = (cexpr1 + many(op_('&') + cexpr1)
         >> (lambda n: make_expr('&', n[0], n[1])))
constraint = op_('<') + cexpr + op_('>')
row = maybe(constraint) + matches >> tuple
tab = (skip(n('tab')) + op_('[') + idref + op_(']') +
       op_('{') + many(row) + op_('}'))
insns_text = many(tab)# >> dict
insns_file = insns_text + skip(finished)

def parse(seq):
    'Sequence(Token) -> object'
    return insns_file.parse(seq)

def output_constraint(f, constraint):
    if isinstance(constraint, Ref):
        lprint('\t\t\t\t'+constraint.to_xml())
    elif constraint[0] == '&':
        for x in constraint[1:]:
            output_constraint(f, x)
    elif constraint[0] == '!':
        lprint('\t\t\t\t<Not>'+constraint[1].to_xml()+'</Not>')

def output(f, tree):
    lprint('<?xml version="1.0"?>', file=f)
    lprint('<Insns>', file=f)
    for id, rows in tree:
        lprint('\t<Table id="%s">' % id, file=f)
        for constraint, matches in rows:
            lprint('\t\t<Row>', file=f)
            if constraint is not None:
                lprint('\t\t\t<Constraints>', file=f)
                output_constraint(f, constraint)
                lprint('\t\t\t</Constraints>', file=f)
            for match in matches:
                lprint('\t\t\t'+'\n\t\t\t'.join(match.to_xml().split('\n')),
                       file=f)
            lprint('\t\t</Row>', file=f)
        lprint('\t</Table>', file=f)
    lprint('</Insns>', file=f)
    lprint('\n<!-- vim: set ts=2 sw=2 sts=2 tw=78 noet: -->')

def main():
    input = open(sys.argv[1]).read()
    #for tok in tokenize(input):
    #    print tok
    tree = insns_file.parse(tokenize(input))
    #import pprint
    #pprint.pprint(tree)
    output(sys.stdout, tree)

if __name__ == '__main__':
    main()

