#ifndef TRANSFORM_CODE_HPP
#define TRANSFORM_CODE_HPP

#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>
#include <array>
#include <set>

std::set <std::string> dataTypes, functions, conditions, loops, ignorable;

// removing comments with a GCC macro:
std::string removeComments(const std::string &path)
{
    std::array <char, 128> buffer;
    std::string result, cmd = "gcc -fpreprocessed -dD -E -P " + path;
    std::unique_ptr <FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe)
        throw std::runtime_error("popen() failed!");

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        result += buffer.data();

    return result;
}

bool skipLine(const std::string &line);
std::string formatLine(const std::string &line);
std::string removeSpaces(const std::string &fullCode)
{
    std::stringstream ss(fullCode);
    std::string line, newCode, result;

    while (getline(ss, line))
        if (!skipLine(line))
            newCode += formatLine(line);

    std::unique_copy(newCode.begin(), newCode.end(), std::back_insert_iterator <std::string> (result),
                     [] (char a, char b) { return isspace(a) && isspace(b);});

    return result;
}

bool skipLine(const std::string &line)
{
    if (line.size() == 0)
        return true;

    int j = 0;

    while (j < line.size() && line[j] == ' ')
        ++j;

    return j == line.size() || line[j] == '#';
}

bool isMathSign(char ch) { return ch == '=' || ch == '+' || ch == '-' || ch == '*' || ch == '/' || ch == '%' || ch == '>' || ch == '<'; }
std::string formatLine(const std::string &line)
{
    std::string formattedLine;
    int j = 0;

    while (j < line.size())
    {
        if (j+1 < line.size() && ((line[j] == '-' && line[j+1] == '>') || (isMathSign(line[j]) && isMathSign(line[j+1]))))
        {
            formattedLine.push_back(' ');
            formattedLine.push_back(line[j]);
            formattedLine.push_back(line[j+1]);
            formattedLine.push_back(' ');
            ++j;
        }
        else if (line[j] != '_' && ispunct(line[j]))
        {
            formattedLine.push_back(' ');
            formattedLine.push_back(line[j]);
            formattedLine.push_back(' ');
        }
        else
            formattedLine.push_back(line[j]);

        ++j;
    }

    return formattedLine;
}

// removing contents of quotes
void removeQuotes(std::string &code)
{
    size_t pos_beg = code.find('"'), pos_end;

    while (pos_beg != std::string::npos)
    {
        pos_end = code.find('"', pos_beg+1);
        code.erase(pos_beg+1, pos_end-pos_beg-1);
        pos_beg = code.find('"', pos_beg+2);
    }

    pos_beg = code.find('\'');

    while (pos_beg != std::string::npos)
    {
        pos_end = code.find('\'', pos_beg+1);
        code.erase(pos_beg+1, pos_end-pos_beg-1);
        pos_beg = code.find('\'', pos_beg+2);
    }
}

std::string processStatement(std::stringstream &ss, std::string &word, std::string &finalCode, bool isConditionOrLoop);
std::string processFunctionCall(std::stringstream &ss);
void processDeclarations(std::stringstream &ss, std::string &word, std::string &finalCode);
void processConditions(std::stringstream &ss, std::string &word, std::string &finalCode);
void processLoops(std::stringstream &ss, std::string &word, std::string &finalCode);
void processTypedefs(std::stringstream &ss, std::string &word);
std::string transformCode(const std::string &code)
{
    std::stringstream ss(code);
    std::string finalCode, word;
    bool inFunction = false;

    while (!ss.eof())
    {
        // dönüştürmeler
        if (ignorable.find(word) != ignorable.end() || word == "")
        {
            ss >> word;

            if (word == ";")
                ss >> word;
        }
        else if (dataTypes.find(word) != dataTypes.end())
            processDeclarations(ss, word, finalCode);
        else if (conditions.find(word) != conditions.end())
            processConditions(ss, word, finalCode);
        else if (loops.find(word) != loops.end())
            processLoops(ss, word, finalCode);
        else if (word == "typedef") // a new type declaration
            processTypedefs(ss, word);
        else if (word == "goto")
        {
            ss >> word;
            ss >> word;
            ss >> word;
        }
        else if (word == "return")
        {
            finalCode += "_r_";
            ss >> word;
        }
        else
        {
            finalCode += processStatement(ss, word, finalCode, false);
            ss >> word;
        }
    }

    return finalCode;
}

void processVariableDeclarations(std::stringstream &ss, std::string &punct, std::string &finalCode);
void processFunctionDeclarations(std::stringstream &ss, std::string &funcName);
void processDeclarations(std::stringstream &ss, std::string &word, std::string &finalCode)
{
    // a data type dedected, it may specify a variable or a function decleration

    if (word == "struct")
        ss >> word;

    std::string name;
    ss >> name; // getting the name of the variable/function

    if (name == "{") // a struct type declaration
        while (word != "}")
            ss >> word;
    else
    {
        // we ignore additional part of data types, like "long" int x, "const static signed" long x...
        // also if there is a pointer or a reference, we ignore it
        if (ignorable.find(name) != ignorable.end() || dataTypes.find(name) != dataTypes.end() || name[0] == '*' || name[0] == '&')
            ss >> name;

        ss >> word; // getting the punctuation after the name

        // if the following punctuation is a comma or a equals sign,
        // it means that we found a variable assignment
        if (word == "," || word == "=" || word == "[")
            processVariableDeclarations(ss, word, finalCode);
        else if (word == "(") //  a function decleration
            processFunctionDeclarations(ss, name);
    }

    ss >> word;
}

bool isNumber(const std::string &str)
{
    if (str[0] == 'e' && str[str.size()-1] == 'e')
        return false;

    int num_of_e = 0; // the number may be in form of 1e10, 1e15, 2e3...

    for (int i = 0; i < str.size(); ++i)
        if (!isdigit(str[i]))
        {
            if (str[i] == 'e')
            {
                ++num_of_e;

                if (num_of_e > 1)
                    return false;
            }
            else
                return false;
        }

    return true;
}

void processVariableDeclarations(std::stringstream &ss, std::string &word, std::string &finalCode)
{
    while (word != ";")
    {
        if (word == ",")
        {
            if (finalCode.back() == '_' || finalCode.back() == ')')
                finalCode += ';';

            ss >> word; // getting the next variable

            if (word[0] == '*' || word[0] == '&') // if there is a pointer or a reference, ignore it
                ss >> word;

            ss >> word;
        }
        else if (word == "=")
        {
            ss >> word;

            if (word == "{")
            {
                int numOfOpenCurlyBraces = 1;
                finalCode += "_v_=_v_";

                while (numOfOpenCurlyBraces > 0)
                {
                    ss >> word;

                    if (word == "{")
                        ++numOfOpenCurlyBraces;
                    else if (word == "}")
                        --numOfOpenCurlyBraces;
                }

                finalCode += ';';
                ss >> word;
            }
            else
                finalCode += "_v_=" + processStatement(ss, word, finalCode, false);
        }
        else if (word == "[")
        {
            ss >> word;

            while (word != "]")
                ss >> word;

            ss >> word;
        }
    }
}

void processFunctionDeclarations(std::stringstream &ss, std::string &funcName)
{
    std::string word;
    ss >> word;

    while (word != ")")
        ss >> word;

    if (functions.find(funcName) == functions.end())
        functions.insert(funcName);

    if (word == ";")
        ss >> word;
}

std::string processStatement(std::stringstream &ss, std::string &word, std::string &finalCode, bool isConditionOrLoop)
{
    std::string statement;
    int numOfOpenParantheses = 1;

    while (word != "," && word != ";" && word != ":" && numOfOpenParantheses > 0)
    {
        if (isConditionOrLoop)
        {
            if (word == "(")
                ++numOfOpenParantheses;
            else if (word == ")")
                --numOfOpenParantheses;
        }

        if (functions.find(word) != functions.end()) // function call
        {
            statement += processFunctionCall(ss);
            ss >> word;
        }
        else if (isMathSign(word[0]) || word == "(" || word == ")" || word == "&" || word == "|" || word == "!" || word == "." || word == "->")
        {
            statement += word;
            ss >> word;
        }
        else if (word == "[")
        {
            int numOfOpenParantheses = 1;

            while (numOfOpenParantheses > 0)
            {
                ss >> word;

                if (word == "[")
                    ++numOfOpenParantheses;
                else if (word == "]")
                    --numOfOpenParantheses;
            }

            ss >> word;
        }
        else
        {
            ss >> word;

            if (word == ":") // a label definition is found
                statement += "_l_()";
            else
                statement += "_v_";
        }
    }

    if (word == ";" && !isConditionOrLoop && statement.size() > 0 && (statement.back() == '_' || statement.back() == ')'))
        statement += ";";

    return statement;
}

std::string processFunctionCall(std::stringstream &ss)
{
    std::string equation = "_f_(", word;
    ss >> word; // '(' character
    ss >> word;

    while (word != ")")
    {
        equation += "_p_";
        ss >> word;

        while (word != ")" && word != ",")
        {
            if (word == "(")
            {
                int numOfOpenParantheses = 1;

                while (numOfOpenParantheses > 0)
                {
                    ss >> word;
                    
                    if (word == "(")
                        ++numOfOpenParantheses;
                    else if (word == ")")
                        --numOfOpenParantheses;
                }
            }
            
            ss >> word;
        }

        if (word == ",")
            equation += ",";
    }

    return equation + ")";
}

void processConditions(std::stringstream &ss, std::string &word, std::string &finalCode)
{
    if (word == "switch")
    {
        ss >> word;
        ss >> word;
        processStatement(ss, word, finalCode, true);
        ss >> word;
    }
    else if (word == "case")
    {
        finalCode += "_c_(_v_==_v_)";
        ss >> word;
        ss >> word;
        ss >> word;
    }
    else if (word == "default")
    {
        finalCode += "_c_()";
        ss >> word;
        ss >> word;
    }
    else
    {
        bool isElse = (word == "else");
        ss >> word;

        if (isElse && word != "if")
            finalCode += "_c_()";
        else
        {
            if (word == "if") // the condition is "else if"
                ss >> word; // getting '(' character

            ss >> word;
            finalCode += "_c_(" + processStatement(ss, word, finalCode, true);
        }
    }
}

void processLoops(std::stringstream &ss, std::string &word, std::string &finalCode)
{
    if (word == "for")
    {
        ss >> word;

        while (word != ";")
            ss >> word;

        ss >> word;

        finalCode += "_l_(" + processStatement(ss, word, finalCode, true) + ")";

        ss >> word;
        processStatement(ss, word, finalCode, true);
    }
    else if (word == "while")
    {
        ss >> word;
        ss >> word;
        finalCode += "_l_(" + processStatement(ss, word, finalCode, true);
    }
    else // do while
    {
        finalCode += "_l_()";
        ss >> word;
    }
}

void processTypedefs(std::stringstream &ss, std::string &word)
{
    ss >> word;

    if (word == "struct") // struct declaration
    {
        while (word != "}")
            ss >> word;

        ss >> word;
        dataTypes.insert(word);
        ss >> word;
    }
    else // a type name abbreviation, like "typedef unsigned long int uli;"
    {
        std::string prev;

        while (word != ";")
        {
            prev = word;
            ss >> word;
        }

        dataTypes.insert(prev);
    }

    ss >> word;
}

void initializeSets()
{
    dataTypes = {"bool", "char", "signed", "unsigned", "short", "int", "long", "size_t", "float", "double", "void", "FILE", "struct"};

    ignorable = {"const", "static", "break", "continue", "{", "}"};

    conditions = {"if", "else", "switch", "case", "default"};

    loops = {"for", "while", "do"};

    functions = {"assert", "isalnum", "isalpha", "iscntrl", "isdigit", "isgraph", "islower", "isprint",
                 "ispunct", "isspace", "isupper", "isxdigit", "tolower", "toupper", "errno", "localeconv",
                 "setlocale", "acos", "asin", "atan", "atan2", "ceil", "cos", "cosh", "exp", "fabs", "floor", "fmod",
                 "frexp", "ldexp", "log", "log10", "modf", "pow", "sin", "sinh", "sqrt", "tan", "tanh", "jmp_buf",
                 "longjmp", "setjmp", "raise", "signal", "sig_atomic_t", "va_arg", "va_end", "va_start",
                 "clearerr", "fclose", "feof", "ferror", "fflush", "fgetc", "fgetpos", "fgets", "fopen",
                 "fprintf", "fputc", "fputs", "fread", "freopen", "fscanf", "fseek", "fsetpos", "ftell",
                 "fwrite", "getchar", "getch", "getc", "main", "gets", "perror", "printf", "putc", "putchar", "puts", "remove",
                 "rename", "rewind", "scanf", "setbuf", "setvbuf", "sprintf", "sscanf", "tmpfile", "tmpnam",
                 "ungetc", "vfprintf", "vprintf", "vsprintf", "abort", "abs", "atexit", "atof", "atoi", "atol",
                 "bsearch", "calloc", "div", "exit", "free", "getenv", "labs", "ldiv", "malloc", "mblen", "mbstowcs",
                 "mbtowc", "qsort", "rand", "realloc", "srand", "strtod", "strtol", "strtoul", "system",
                 "wcstombs", "wctomb", "memchr", "memcmp", "memcpy", "memmove", "memset", "strcat", "strchr",
                 "strcmp",", ""strcoll", "strcpy", "strcspn", "strerror", "strlen", "strncat", "strncmp",
                 "strncpy", "strpbrk", "strrchr", "strspn", "strstr", "strtok", "strxfrm", "asctime",
                 "clock", "ctime", "difftime", "gmtime", "localtime", "mktime", "strftime", "time", "sizeof"};
}

#endif