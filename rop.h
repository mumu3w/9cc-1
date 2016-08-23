
// id   name

_rop(IR_NONE,           "null")

// control
_rop(IR_LABEL,          "label")
_rop(IR_GOTO,           "goto")
_rop(IR_IF_I,           "if")
_rop(IR_IF_F,           "if")
_rop(IR_IF_FALSE_I,     "ifFalse")
_rop(IR_IF_FALSE_F,     "ifFalse")
_rop(IR_RETURNI,        "return")
_rop(IR_RETURNF,        "return")

// binary

// arith
_rop(IR_ADDI,           "+")
_rop(IR_ADDF,           "+")
                           
_rop(IR_SUBI,           "-")
_rop(IR_SUBF,           "-")
                           
_rop(IR_DIVI,           "/")
_rop(IR_IDIVI,          "/")
_rop(IR_DIVF,           "/")
                           
_rop(IR_MULI,           "*")
_rop(IR_IMULI,          "*")
_rop(IR_MULF,           "*")
                           
// integer                 
_rop(IR_MOD,            "%")
_rop(IR_IMOD,           "%")
_rop(IR_OR,             "|")
_rop(IR_AND,            "&")
_rop(IR_XOR,            "^")
_rop(IR_LSHIFT,         "<<")
_rop(IR_ILSHIFT,        "<<")
_rop(IR_RSHIFT,         ">>")
_rop(IR_IRSHIFT,        ">>")

_rop(IR_ASSIGNI,        "=")
_rop(IR_ASSIGNF,        "=")

// unary
_rop(IR_NOT,            "~")
_rop(IR_MINUSI,         "-")
_rop(IR_MINUSF,         "-")
_rop(IR_ADDRESS,        "&")

//
_rop(IR_SUBSCRIPT,      "[]")
_rop(IR_INDIRECTION,    "*")

// function
_rop(IR_CALL,           "call")
_rop(IR_PARAM,          "param")

// conv
_rop(IR_CONV_UI_UI,     "uint => uint")
_rop(IR_CONV_SI_SI,     "int => int")
_rop(IR_CONV_UI_SI,     "uint => int")
_rop(IR_CONV_SI_UI,     "int => uint")

_rop(IR_CONV_SI_F,      "int => float")
_rop(IR_CONV_UI_F,      "uint => float")

_rop(IR_CONV_FF,        "float => float")

_rop(IR_CONV_F_UI,      "float => uint")
_rop(IR_CONV_F_SI,      "float => int")

_rop(IR_CONV_P_B,       "ptr => bool")
_rop(IR_CONV_I_B,       "int => bool")
_rop(IR_CONV_F_B,       "float => bool")

#undef _rop