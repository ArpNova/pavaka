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

    vector<Token>tokens = tokenize(contents);
    

    return EXIT_SUCCESS;
}