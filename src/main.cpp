#include <iostream>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "generation.hpp"

using namespace std;

int main(int argc, char *argv[])
{
    string input_path;
    string output_path = "out";

    for (int i = 1; i < argc; ++i)
    {
        string arg = argv[i];
        if (arg == "-o")
        {
            if (i + 1 < argc)
            {
                output_path = argv[++i];
            }
            else
            {
                cerr << "Error: -o option requires an argument" << endl;
                return EXIT_FAILURE;
            }
        }
        else if (arg == "-h" || arg == "--help")
        {
            cout << "Usage: pavakac <input.pavaka> [-o <output>]" << endl;
            return EXIT_SUCCESS;
        }
        else if (!arg.empty() && arg[0] == '-')
        {
            cerr << "Unknown option: " << arg << endl;
            return EXIT_FAILURE;
        }
        else
        {
            if (input_path.empty())
            {
                input_path = arg;
            }
            else
            {
                cerr << "Error: Multiple input files provided: " << input_path << ", " << arg << endl;
                return EXIT_FAILURE;
            }
        }
    }

    if (input_path.empty())
    {
        cerr << "Incorrect usage... correct usage is -" << endl
             << "pavakac <input.pavaka> [-o <output>]" << endl;
        return EXIT_FAILURE;
    }

    string contents;
    {
        stringstream contents_stream;
        fstream input(input_path, ios::in);
        if (!input)
        {
            cerr << "Could not open input file: " << input_path << endl;
            return EXIT_FAILURE;
        }
        contents_stream << input.rdbuf();
        contents = contents_stream.str();
    }

    Tokenizer tokenizer(std::move(contents));
    vector<Token> tokens = tokenizer.tokenize();

    Parser parser(std::move(tokens));
    std::optional<NodeProg> prog = parser.parse_prog();

    if (!prog.has_value())
    {
        cerr << "Invalid program" << endl;
        return EXIT_FAILURE;
    }

    Generator generator(prog.value());

    string asm_file = output_path + ".asm";
    string obj_file = output_path + ".o";

    {
        fstream file(asm_file, ios::out);
        if (!file)
        {
            cerr << "Could not create " << asm_file << endl;
            return EXIT_FAILURE;
        }
        file << generator.gen_prog();
        if (!file)
        {
            cerr << "Could not write " << asm_file << endl;
            return EXIT_FAILURE;
        }
    }

    string nasm_cmd = "nasm -felf64 \"" + asm_file + "\" -o \"" + obj_file + "\"";
    if (system(nasm_cmd.c_str()) != 0)
    {
        cerr << "Assembler failed" << endl;
        return EXIT_FAILURE;
    }

    string ld_cmd = "ld -o \"" + output_path + "\" \"" + obj_file + "\"";
    if (system(ld_cmd.c_str()) != 0)
    {
        cerr << "Linker failed" << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}