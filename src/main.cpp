#include <iostream>

using namespace std;

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        cerr << "incorrect usage... correct usage is -" << endl
             << "void <input.p>" << endl;
        return EXIT_FAILURE;
    }
    cout << "Hello World!" << endl;
    return EXIT_SUCCESS;
}