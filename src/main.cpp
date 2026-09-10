#include <iostream>
#include <fstream>
#include <sstream>
#include <optional>
#include <string>
#include <vector>

using namespace std;

enum class TokenType
{
    _return,
    int_lit,
    semi
};

struct Token
{
    TokenType type;
    optional<string> value;
};

vector<Token> tokenize(const string &str)
{
    vector<Token> tokens;
    string buf;

    for (int i = 0; i < str.length(); i++)
    {
        char c = str.at(i);

        if (isalpha(c))
        {
            buf.push_back(c);
            i++;
            while (isalnum(str.at(i)))
            {
                buf.push_back(str.at(i));
                i++;
            }
            i--;

            if (buf == "return")
            {
                tokens.push_back({.type = TokenType::_return});
                buf.clear();
            }
            else
            {
                cerr << "you messed up!" << endl;
                exit(EXIT_FAILURE);
            }
        }
        else if (isdigit(c))
        {
            buf.push_back(c);
            i++;
            while (isdigit(str.at(i)))
            {
                buf.push_back(str.at(i));
                i++;
            }
            tokens.push_back({.type = TokenType::int_lit, .value = buf});
            buf.clear();
            i--;
        }
        else if (c == ';')
        {
            tokens.push_back({.type = TokenType::semi});
        }
        else if (isspace(c))
        {
            continue;
        }
        else
        {
            cerr << "you messed up!" << endl;
            exit(EXIT_FAILURE);
        }
    }

    return tokens;
}

string token_to_asm(const vector<Token> &tokens)
{
    stringstream output;
    output << "global _start\n_start:\n";

    for (int i = 0; i < tokens.size(); i++)
    {
        const Token &token = tokens.at(i);
        if (token.type == TokenType::_return)
        {
            if (i + 1 < tokens.size() && tokens.at(i + 1).type == TokenType::int_lit)
            {
                if (i + 2 < tokens.size() && tokens.at(i + 2).type == TokenType::semi)
                {
                    output << "    mov rax, 60\n";
                    output << "    mov rdi," << tokens.at(i + 1).value.value() << endl;
                    output << "    syscall";
                }
            }
        }
    }
    return output.str();
}

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

    cout << contents << endl;

    vector<Token> tokens = tokenize(contents);

    {
        fstream file("out.asm", ios::out);
        file << token_to_asm(tokens);
    }

    // for (const auto &token : tokens)
    // {
    //     if (token.type == TokenType::_return)
    //         cout << "RETURN\n";

    //     else if (token.type == TokenType::int_lit)
    //         cout << "INT: " << token.value.value() << "\n";

    //     else if (token.type == TokenType::semi)
    //         cout << "SEMI\n";
    // }

    return EXIT_SUCCESS;
}