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
        file << generator.gen_prog();
    }

    system("nasm -felf64  out.asm");
    system("ld -o out out.o");

    return EXIT_SUCCESS;
}