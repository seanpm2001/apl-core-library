/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */


#include <fstream>
#include <regex>

#include "enumparser.h"

bool
isMatch(const std::string& name, const std::vector<std::string>& filter)
{
    if (filter.empty())
        return true;

    for (const std::string& pattern : filter) {
        try {
            if (std::regex_match(name, std::regex(pattern)))
                return true;
        }
        catch (std::regex_error e) {
            std::cerr << "Can't match with pattern: " << e.what() << std::endl;
        }
    }

    return false;
}

void
generateTypescript(const enums::EnumMap& enumerations,
                   const std::string& writeDir,
                   const std::vector<std::string>& filter)
{
    std::ofstream out;

    for (const auto& m : enumerations) {
        const auto& name = m.first;
        const auto& values = m.second;

        if (isMatch(name, filter)) {
            out.open(writeDir + "/" + name + ".ts");
            enums::writeTypeScript(out, name, values);
            out.close();
        }
    }
}

void
generateJava(const enums::EnumMap& enumerations,
             const std::string& writeDir,
             const std::string& package,
             const std::vector<std::string>& filter)
{
    std::ofstream out;

    out.open(writeDir + "/APLEnum.java");
    out << "/*\n"
        << " * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.\n"
        << " */\n\n"
        << "/*\n"
        <<  " * AUTOGENERATED FILE. DO NOT MODIFY!\n"
        << " * This file is autogenerated by enumgen.\n"
        << " */\n\n"
        << "package " << package << ";\n\n"
        << "public interface APLEnum {\n"
        << "    int getIndex();\n"
        << "}";
    out.close();

    for (const auto& m : enumerations) {
        const auto& name = m.first;
        const auto& values = m.second;

        if (isMatch(name, filter)) {
            out.open(writeDir + "/" + name + ".java");
            enums::writeJava(out, package, name, values);
            out.close();
        }
    }
}

static void usage(std::string msg="")
{
    if (!msg.empty())
        std::cout << msg << std::endl;

    std::cout <<
        R"(
Usage: enumgen [options] [--] FILE+

  Generate enums for a target language.  Files should be in C/C++

  Options:
     -o,--output PATH           Path to generated output directory. Defaults to current directory.
                                The directory must already exist (enumgen will not create a new directory)
     -f,--filter STRING         Regex filter.  Accepted multiple times.
     -p,--package PACKAGE       Java package.
     -l,--language LANGUAGE     Language to generate.  May be 'typescript' or 'java'.  Required.
     -v, --verbose              Verbose mode (may be repeated for additional information)
     --version                  Displays version information and exist
     --, --ignore_rest          All arguments following this one are treated as files
     -h,--help                  Print this help
)";
    exit(msg.empty() ? 0 : 1);
}


int
main(int argc, char** argv) {
    std::string out(".");
    std::vector<std::string> filter;
    std::string package;
    std::string language;

    std::vector<std::string> args(argv + 1, argv + argc);
    for (auto iter = args.begin(); iter != args.end() && !iter->empty() && iter->front() == '-' ; ) {
        auto takeOne = [&](std::string name) {
            iter = args.erase(iter);
            if (iter == args.end()) {
                std::cerr << name << " expects a value" << std::endl;
                exit(1);
            }
            auto result = *iter;
            iter = args.erase(iter);
            return result;
        };

        if (*iter == "-h" || *iter == "--help") {
            usage();
        } else if (*iter == "--version") {
            std::cout << argv[0] << "  version: 1.0" << std::endl;
            return 0;
        } else if (*iter == "-o" || *iter == "--output") {
            out = takeOne("output");
        } else if (*iter == "-f" || *iter == "--filter") {
            filter.emplace_back(takeOne("filter"));
        } else if (*iter == "-p" || *iter == "--package") {
            package = takeOne("package");
        } else if (*iter == "-l" || *iter == "--language") {
            language = takeOne("language");
        } else if (*iter == "-v" || *iter == "--verbose") {
            iter = args.erase(iter);
            enums::EnumParser::sVerbosity++;
        } else if (*iter == "--" || *iter == "--ignore_rest") {
            args.erase(iter);
            break;
        } else {
            std::cerr << "Unrecognized option " << *iter << std::endl;
            return 1;
        }
    }

    if (language != "typescript" && language != "java") {
        std::cerr << "Language must be 'typescript' or 'java'" << std::endl;
        return 1;
    }

    if (language == "java" && package.empty()) {
        std::cerr << "You must supply the java package with the -p option" << std::endl;
        return 1;
    }

    if (enums::EnumParser::sVerbosity) {
        std::cerr << "out='" << out << "'" << std::endl
                  << "package='" << package << "'" << std::endl
                  << "language='" << language << "'" << std::endl
                  << "filter=";
        for (const auto& m : filter)
            std::cerr << m << " ";
        std::cerr << std::endl << "files=";
        for (const auto& m : args)
            std::cerr << m << " ";
        std::cerr << std::endl;
    }

    // All of the remaining arguments in the "args" array are input files
    enums::EnumParser parser;
    for (const auto& filePath : args)
        parser.add(std::ifstream(filePath), filePath);

    if (language == "typescript")
        generateTypescript(parser.enumerations(), out, filter);
    else if (language == "java")
        generateJava(parser.enumerations(), out, package, filter);

    return 0;
}

