#include "stabs_parser/StabsParser.h"
#include "gtest/gtest.h"
#include <array>

struct StabsParserTest : ::testing::Test {
protected:
    template <size_t size>
    void Test(const char* (&source)[size]) {
        for (auto s : source) {
            EXPECT_TRUE(m_parser.Parse(s));
        }
    }

    void Test(const char* line) { EXPECT_TRUE(m_parser.Parse(line)); }

    StabsParser m_parser;
};

TEST_F(StabsParserTest, Ignored) {
    const char* source[] = {
        R"(                             26 ;	.stabs	"main.cpp",100,0,0,Ltext0)",
        R"(                             29 ;	.stabs	"gcc2_compiled.",60,0,0,0)"
        //
    };
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, Multiline) {
    // struct MyStruct {
    //    int aaaaaaaaaaaaaaaaaaaaaaaaa;
    //    int bbbbbbbbbbbbbbbbbbbbbbbbb;
    //    int ccccccccccccccccccccccccc;
    //    int ddddddddddddddddddddddddd;
    //    int eeeeeeeeeeeeeeeeeeeeeeeee;
    //    int fffffffffffffffffffffffff;
    //    int ggggggggggggggggggggggggg;
    //    int hhhhhhhhhhhhhhhhhhhhhhhhh;
    //    int iiiiiiiiiiiiiiiiiiiiiiiii;
    //};
    const char* source =
        R"(
                             59 ;	.stabs	"MyStruct:T25=s9aaaaaaaaaaaaaaaaaaaaaaaaa:7,0,8;bbbbbbbbbbbbbbbbbbbbbbbbb:7,8,8;\\",128,0,0,0
                             60 ;	.stabs	"ccccccccccccccccccccccccc:7,16,8;ddddddddddddddddddddddddd:7,24,8;\\",128,0,0,0
                             61 ;	.stabs	"eeeeeeeeeeeeeeeeeeeeeeeee:7,32,8;fffffffffffffffffffffffff:7,40,8;\\",128,0,0,0
                             62 ;	.stabs	"ggggggggggggggggggggggggg:7,48,8;hhhhhhhhhhhhhhhhhhhhhhhhh:7,56,8;\\",128,0,0,0
                             63 ;	.stabs	"iiiiiiiiiiiiiiiiiiiiiiiii:7,64,8;;",128,0,0,0

)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, PrimitiveTypeDefinition) {
    const char* source =
        R"(
                             31 ;	.stabs	"complex long double:t3=R3;8;0;",128,0,0,0
                             32 ;	.stabs	"complex double:t4=R3;8;0;",128,0,0,0
                             33 ;	.stabs	"complex float:t5=R3;8;0;",128,0,0,0
                             35 ;	.stabs	"long long unsigned int:t8=r8;0;4294967295;",128,0,0,0
                             36 ;	.stabs	"long unsigned int:t9=r9;0;65535;",128,0,0,0
                             37 ;	.stabs	"unsigned int:t10=r10;0;255;",128,0,0,0
                             38 ;	.stabs	"long long int:t11=r11;-2147483648;2147483647;",128,0,0,0
                             39 ;	.stabs	"long int:t12=r12;-32768;32767;",128,0,0,0
                             40 ;	.stabs	"int:t7",128,0,0,0
                             41 ;	.stabs	"char:t13=r13;0;127;",128,0,0,0
                             42 ;	.stabs	"signed:t7",128,0,0,0
                             43 ;	.stabs	"unsigned long:t9",128,0,0,0
                             44 ;	.stabs	"long long unsigned:t8",128,0,0,0
                             45 ;	.stabs	"short int:t14=r14;-128;127;",128,0,0,0
                             46 ;	.stabs	"short unsigned int:t15=r15;0;255;",128,0,0,0
                             47 ;	.stabs	"unsigned short:t15",128,0,0,0
                             48 ;	.stabs	"signed char:t16=r16;-128;127;",128,0,0,0
                             49 ;	.stabs	"unsigned char:t17=r17;0;255;",128,0,0,0
                             50 ;	.stabs	"float:t18=r7;4;0;",128,0,0,0
                             51 ;	.stabs	"double:t19=r7;4;0;",128,0,0,0
                             52 ;	.stabs	"long double:t20=r7;4;0;",128,0,0,0
                             53 ;	.stabs	"void:t2",128,0,0,0
                             54 ;	.stabs	"wchar_t:t21=r21;-128;127;",128,0,0,0
                             55 ;	.stabs	"bool:t22=eFalse:0,True:1,;",128,0,0,0
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, DISABLED_PrimitiveTypeDefinition2) {
    const char* source =
        R"(
                             30 ;	.stabs	"__builtin_va_list:t1=*2=2",128,0,0,0
                             34 ;	.stabs	"complex int:t6=s2real:7=r7;-128;127;,0,8;imag:7,8,8;;",128,0,0,0
                             56 ;	.stabs	"__vtbl_ptr_type:t23=*24=f7",128,0,0,0
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, AliasTypeDefinition) {
    // typedef unsigned int uint8_t;
    // typedef signed int int8_t;
    // typedef unsigned long int uint16_t;
    // typedef signed long int int16_t;
    // typedef uint16_t size_t;
    const char* source =
        R"(
                             59 ;	.stabs	"uint8_t:t25=10",128,0,0,0
                             60 ;	.stabs	"int8_t:t26=7",128,0,0,0
                             61 ;	.stabs	"uint16_t:t27=9",128,0,0,0
                             62 ;	.stabs	"int16_t:t28=12",128,0,0,0
                             63 ;	.stabs	"size_t:t29=27",128,0,0,0
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, EnumTypeDefinition) {
    // enum WeekDay { Monday, Tuesday, Wednesday, EndOfDays = Wednesday, Foo = -5000 };
    // enum { NoName1, NoName2 };
    // enum { NoName3, NoName4 };
    const char* source =
        R"(
                             59 ;	.stabs	"WeekDay:t25=eMonday:0,Tuesday:1,Wednesday:2,EndOfDays:2,\\",128,0,0,0
                             60 ;	.stabs	"Foo:-5000,;",128,0,0,0
                             61 ;	.stabs	"$_0:t26=eNoName1:0,NoName2:1,;",128,0,0,0
                             62 ;	.stabs	"$_1:t27=eNoName3:0,NoName4:1,;",128,0,0,0
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, ArrayDefinition) {
    // int a[10];
    // int b[8][7];
    // int c[6];
    // int d[7];
    // int e[8][9];
    // int f[8][9];
    // int f2[6][7][8][9];
    // int* g[10][11][12];
    // int h[11][12];
    const char* source =
        R"(
                             97 ;	.stabs	"a:26=ar27=r27;0;-1;;0;9;7",128,0,0,6009
                             98 ;	.stabs	"b:28=ar27;0;7;29=ar27;0;6;7",128,0,0,5940
                             99 ;	.stabs	"c:30=ar27;0;5;7",128,0,0,6003
                            100 ;	.stabs	"d:29",128,0,0,5996
                            101 ;	.stabs	"e:31=ar27;0;7;32=ar27;0;8;7",128,0,0,5868
                            102 ;	.stabs	"f:31",128,0,0,5796
                            103 ;	.stabs	"f2:33=ar27;0;5;34=ar27;0;6;31",128,0,0,0
                            104 ;	.stabs	"g:35=ar27;0;9;36=ar27;0;10;37=ar27;0;11;38=*7",128,0,0,3024
                            105 ;	.stabs	"h:39=ar27;0;10;40=ar27;0;11;7",128,0,0,5664
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, StructTypeDefinition) {
    // struct MyStruct {
    //     int a;
    //     float b;
    //     bool c;
    //     int* d;
    //     MyStruct* e;
    // };
    const char* source =
        R"(
                             59 ;	.stabs	"MyStruct:T25=s10a:7,0,8;b:18,8,32;c:22,40,8;d:26=*7,48,16;\\",128,0,0,0
                             60 ;	.stabs	"e:27=*25,64,16;;",128,0,0,0
                             61 ;	.stabs	"MyStruct:t25",128,0,0,0
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, StructTypeDefinitionWithStaticMember) {
    // struct MyStruct {
    //    static int static_member;
    //};
    const char* source =
        R"(
                             59 ;	.stabs	"MyStruct:T25=s1static_member:7,0,0;;",128,0,0,0
                             60 ;	.stabs	"MyStruct:t25",128,0,0,0
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, StructTypeDefinitionWithArrayMember) {
    // struct MyStruct {
    //    int array_member[10];
    //};
    const char* source =
        R"(
                             59 ;	.stabs	"MyStruct:T25=s10array_member:26=ar27=r27;0;-1;;0;9;7,0,80;;",128,0,0,0
                             60 ;	.stabs	"MyStruct:t25",128,0,0,0
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, IncludeFile) {
    const char* source =
        R"(
                             64 ;	.stabs	"src/dummy.h",132,0,0,Ltext1
                             80 ;	.stabs	"src/main.cpp",132,0,0,Ltext2
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, SectionSymbolLabel) {
    // const int var_const = 0;
    // static int var_init = 0;
    // static int var_noinit;
    // void foo() {
    //    static int var_static_local = 0;
    //    var_static_local = 1;
    //}
    // int main() {}
    const char* source =
        R"(
                             92 ;	.stabs	"foo():F2",36,0,0,__Z3foov
                             93 ;	.stabs	"var_static_local:V7",38,0,0,__ZZ3foovE16var_static_local

                            105 ;	.stabs	"main:F7",36,0,0,_main

                            111 ;	.stabs	"var_const:S7",36,0,0,__ZL9var_const
                            112 ;	.stabs	"var_init:S7",38,0,0,__ZL8var_init
                            113 ;	.stabs	"var_noinit:S7",40,0,0,__ZL10var_noinit
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, FunctionLocalVariable) {
    // int foo() {
    //    int var_no_init;
    //    int var_init = 1;
    //    int* var_ptr = &var_init;
    //}
    const char* source =
        R"(

                             79 ;	.stabs	"var_no_init:7",128,0,0,2
                             80 ;	.stabs	"var_init:7",128,0,0,1
                             81 ;	.stabs	"var_ptr:25=*7",128,0,0,3
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, DISABLED_FunctionParameter) {
    // int foo(int p1, int* p2) {
    //}
    const char* source =
        R"(
                             70 ;	.stabs	"p1:p7",160,0,0,3
                             71 ;	.stabs	"p2:p25=*7",160,0,0,1
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

TEST_F(StabsParserTest, DISABLED_GlobalVariable) {
    // int global_var = 0;
    // int* global_ptr = &global_var;
    const char* source =
        R"(
                             66 ;	.stabs	"global_var:G7",32,0,0,0
                             67 ;	.stabs	"global_ptr:G25=*7",32,0,0,0
)";
    ASSERT_NO_FATAL_FAILURE(Test(source));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
