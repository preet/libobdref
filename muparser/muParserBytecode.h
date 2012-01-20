/*
                 __________                                      
    _____   __ __\______   \_____  _______  ______  ____ _______ 
   /     \ |  |  \|     ___/\__  \ \_  __ \/  ___/_/ __ \\_  __ \
  |  Y Y  \|  |  /|    |     / __ \_|  | \/\___ \ \  ___/ |  | \/
  |__|_|  /|____/ |____|    (____  /|__|  /____  > \___  >|__|   
        \/                       \/            \/      \/        
  Copyright (C) 2004-2011 Ingo Berg

  Permission is hereby granted, free of charge, to any person obtaining a copy of this 
  software and associated documentation files (the "Software"), to deal in the Software
  without restriction, including without limitation the rights to use, copy, modify, 
  merge, publish, distribute, sublicense, and/or sell copies of the Software, and to 
  permit persons to whom the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all copies or 
  substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
  NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND 
  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, 
  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. 
*/
#ifndef MU_PARSER_BYTECODE_H
#define MU_PARSER_BYTECODE_H

#include <cassert>
#include <string>
#include <stack>
#include <vector>

#include "muParserDef.h"
#include "muParserError.h"
#include "muParserToken.h"

/** \file
    \brief Definition of the parser bytecode class.
*/


namespace mu
{
  struct SToken
  {
    ECmdCode Cmd;
    int StackPos;

    union
    {
      union //SValData
      {
        value_type *ptr;
        value_type data;
      } Val;

      struct //SFunData
      {
        void* ptr;
        int   argc;
        int   idx;
      } Fun;

      struct //SOprtData
      {
        value_type *ptr;
        int offset;
      } Oprt;
    };
  };
  
  
  /** \brief Bytecode implementation of the Math Parser.

  The bytecode contains the formula converted to revers polish notation stored in a continious
  memory area. Associated with this data are operator codes, variable pointers, constant 
  values and function pointers. Those are necessary in order to calculate the result.
  All those data items will be casted to the underlying datatype of the bytecode.

  \author (C) 2004-2011 Ingo Berg 
*/
class ParserByteCode
{
private:

    /** \brief Token type for internal use only. */
    typedef ParserToken<value_type, string_type> token_type;

    /** \brief Token vector for storing the RPN. */
    typedef std::vector<SToken> rpn_type;

    /** \brief Position in the Calculation array. */
    unsigned m_iStackPos;

    /** \brief Maximum size needed for the stack. */
    std::size_t m_iMaxStackSize;
    
    /** \brief The actual rpn storage. */
    rpn_type  m_vRPN;

public:

    ParserByteCode();
    ParserByteCode(const ParserByteCode &a_ByteCode);
    ParserByteCode& operator=(const ParserByteCode &a_ByteCode);
    void Assign(const ParserByteCode &a_ByteCode);

    void AddVar(value_type *a_pVar);
    void AddVal(value_type a_fVal);
    void AddOp(ECmdCode a_Oprt);
    void AddIfElse(ECmdCode a_Oprt);
    void AddAssignOp(value_type *a_pVar);
    void AddFun(void *a_pFun, int a_iArgc);
    void AddBulkFun(void *a_pFun, int a_iArgc);
    void AddStrFun(void *a_pFun, int a_iArgc, int a_iIdx);

    void Finalize();
    void clear();
    std::size_t GetMaxStackSize() const;

    const SToken* GetBase() const;
    //rpn_type Clone() const
    //{
    //  return rpn_type(m_vRPN);
    //}

    void RemoveValEntries(unsigned a_iNumber);
    void AsciiDump();
};

} // namespace mu

#endif


