#include <iostream>
#include <fstream>
#include <sstream>
#include <optional>
#include <string>
#include <vector>

#include "generation.hpp"

using namespace std;



int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "incorrect usage... correct usage is -" << endl
             << "void <input.pn>" << endl;
        return EXIT_FAILURE;
    }
    string contents;
    {
        stringstream contents_stream;
        fstream input(argv[1], ios::in);
        if (!input)
        {
            cerr << "Could not open input file: " << argv[1] << endl;
            return EXIT_FAILURE;
        }
        contents_stream << input.rdbuf();
        contents = contents_stream.str();
    }

    Tokenizer tokenizer(std::move(contents));
    vector<Token> tokens = tokenizer.tokenize();

    Parser parser(std::move(tokens));
    std::optional<NodeProg> prog = parser.parse_prog();

    if(!prog.has_value()){
        std::cerr << "Invalid program" << std::endl;
        exit(EXIT_FAILURE);
    }

    Generator generator(prog.value());

    {
        fstream file("out.asm", ios::out);
        if (!file)
        {
            cerr << "Could not create out.asm" << endl;
            return EXIT_FAILURE;
        }
        file << generator.gen_prog();
        if (!file)
        {
            cerr << "Could not write out.asm" << endl;
            return EXIT_FAILURE;
        }
    }

    if (system("nasm -felf64 out.asm") != 0)
    {
        cerr << "Assembler failed" << endl;
        return EXIT_FAILURE;
    }
    if (system("ld -o out out.o") != 0)
    {
        cerr << "Linker failed" << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}