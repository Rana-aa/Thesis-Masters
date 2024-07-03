
import sys

import pyparsing as pp
from pyparsing import pyparsing_common as ppc

LPAREN, RPAREN, LBRACE, RBRACE, LBROK, RBROK, COLON, SEMICOLON, EQUALS, COMMA = map(pp.Suppress, '(){}<>:;=,')

parse_suffix_int = lambda lit: int(lit[:-1]) * (10**(3*(1 + 'kmgtpe'.find(lit[-1].lower()))))
si_suffix = pp.oneOf('k m g t p e', caseless=True)

numeric_literal = pp.Regex('0x[0-9a-fA-F]+').setName('hex int').setParseAction(pp.tokenMap(int, 16)) \
        | (pp.Regex('[0-9]+[kKmMgGtTpPeE]')).setName('size int').setParseAction(pp.tokenMap(parse_suffix_int)) \
        | pp.Word(pp.nums).setName('int').setParseAction(pp.tokenMap(int))
access_def = pp.Regex('[rR]?[wW]?[xX]?').setName('access literal').setParseAction(pp.tokenMap(str.lower))

origin_expr = pp.Suppress(pp.CaselessKeyword('ORIGIN')) + EQUALS + numeric_literal
length_expr = pp.Suppress(pp.CaselessKeyword('LENGTH')) + EQUALS + numeric_literal
mem_expr = pp.Group(ppc.identifier + LPAREN + access_def + RPAREN + COLON + origin_expr + COMMA + length_expr)
mem_contents = pp.ZeroOrMore(mem_expr)

mem_toplevel = pp.CaselessKeyword("MEMORY") + pp.Group(LBRACE + pp.Optional(mem_contents, []) + RBRACE)

glob = pp.Word(pp.alphanums + '._*')
match_expr = pp.Forward()
assignment = pp.Forward()
funccall = pp.Group(pp.Word(pp.alphas + '_') + LPAREN + (assignment | numeric_literal | match_expr | glob | ppc.identifier) + RPAREN + pp.Optional(SEMICOLON))
value = numeric_literal | funccall | ppc.identifier | '.'
formula = (value + pp.oneOf('+ = * / %') + value) | value
# suppress stray semicolons
assignment << (SEMICOLON | pp.Group((ppc.identifier | '.') + EQUALS + (formula | value) + pp.Optional(SEMICOLON)))
match_expr << (glob + LPAREN + pp.OneOrMore(funccall | glob) + RPAREN)

section_contents = pp.ZeroOrMore(assignment | funccall | match_expr);

section_name = pp.Regex('\.[a-zA-Z0-9_.]+')
section_def = pp.Group(section_name + pp.Optional(numeric_literal) + COLON + LBRACE + pp.Group(section_contents) +
        RBRACE + pp.Optional(RBROK + ppc.identifier + pp.Optional('AT' + RBROK + ppc.identifier)))
sec_contents = pp.ZeroOrMore(section_def | assignment)

sections_toplevel = pp.Group(pp.CaselessKeyword("SECTIONS").suppress() + LBRACE + sec_contents + RBRACE)

toplevel_elements = mem_toplevel | funccall | sections_toplevel | assignment
ldscript = pp.Group(pp.ZeroOrMore(toplevel_elements))
ldscript.ignore(pp.cppStyleComment)

if __name__ == '__main__':
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('linker_script', type=argparse.FileType('r'))
    args = parser.parse_args()

    #print(mem_expr.parseString('FLASH (rx) : ORIGIN = 0x0800000, LENGTH = 512K', parseAll=True))
    # print(ldscript.parseString('''
    #     /* Entry Point */
    #     ENTRY(Reset_Handler)
    #
    #     /* Highest address of the user mode stack */
    #     _estack = 0x20020000;    /* end of RAM */
    #     /* Generate a link error if heap and stack don't fit into RAM */
    #     _Min_Heap_Size = 0x200;;      /* required amount of heap  */
    #     _Min_Stack_Size = 0x400;; /* required amount of stack */
    #     ''', parseAll=True))

    print(ldscript.parseFile(args.linker_script, parseAll=True))
    #print(funccall.parseString('KEEP(*(.isr_vector))'))
    #print(section_contents.parseString('''
    #        . = ALIGN(4);
    #        KEEP(*(.isr_vector)) /* Startup code */
    #        . = ALIGN(4);
    #        ''', parseAll=True))

    #print(section_def.parseString('''
    #      .text :
    #      {
    #        . = ALIGN(4);
    #        *(.text)           /* .text sections (code) */
    #        *(.text*)          /* .text* sections (code) */
    #        *(.glue_7)         /* glue arm to thumb code */
    #        *(.glue_7t)        /* glue thumb to arm code */
    #        *(.eh_frame)
    #
    #        KEEP (*(.init))
    #        KEEP (*(.fini))
    #
    #        . = ALIGN(4);
    #        _etext = .;        /* define a global symbols at end of code */
    #      } >FLASH
    #      ''', parseAll=True))

    #print(section_def.parseString('.ARM.extab   : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH', parseAll=True))

    #print(assignment.parseString('__preinit_array_start = .', parseAll=True))
    #print(assignment.parseString('a = 23', parseAll=True))
    #print(funccall.parseString('foo (a=23)', parseAll=True))
    #print(funccall.parseString('PROVIDE_HIDDEN (__preinit_array_start = .);', parseAll=True))
    #print(section_def.parseString('''
    #      .preinit_array     :
    #      {
    #        PROVIDE_HIDDEN (__preinit_array_start = .);
    #        KEEP (*(.preinit_array*))
    #        PROVIDE_HIDDEN (__preinit_array_end = .);
    #        } >FLASH''', parseAll=True))
    #print(match_expr.parseString('*(SORT(.init_array.*))', parseAll=True))
    #print(funccall.parseString('KEEP (*(SORT(.init_array.*)))', parseAll=True))
    #print(section_def.parseString('''
    #      .init_array :
    #      {
    #        PROVIDE_HIDDEN (__init_array_start = .);
    #        KEEP (*(SORT(.init_array.*)))
    #        KEEP (*(.init_array*))
    #        PROVIDE_HIDDEN (__init_array_end = .);
    #      } >FLASH
    #      ''', parseAll=True))

    #print(match_expr.parseString('*(.ARM.extab* .gnu.linkonce.armextab.*)', parseAll=True))
    #print(formula.parseString('. + _Min_Heap_Size', parseAll=True))
    #print(assignment.parseString('. = . + _Min_Heap_Size;', parseAll=True))
    #print(sections_toplevel.parseString('''
    #    SECTIONS
    #    {
    #      .ARMattributes : {  }
    #    }
    #      ''', parseAll=True))
    #sys.exit(0)

