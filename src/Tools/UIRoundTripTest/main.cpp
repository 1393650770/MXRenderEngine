// UIRoundTripTest — round-trip idempotence harness for the UIDocumentModel
// (RML/RCSS parse → serialize → parse → serialize must be byte-identical on
// the second pass). Regression guard for the editor's document model.
//
// Usage: UIRoundTripTest file.rml [file.rcss] ... — exit 0 on all-clean.

#include "UI/UIDocumentModel/RmlParser.h"
#include "UI/UIDocumentModel/UIRcssParser.h"
#include "UI/UIDocumentModel/UIDocumentSerializer.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

std::string ReadFile(const fs::path& path)
{
	std::ifstream in(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

int failures = 0;

void CheckRml(const fs::path& path)
{
	std::string text = ReadFile(path);
	auto doc = MXRender::UI::UIDocModel::RmlParser::Parse(text, path.string());
	for (const auto& e : doc.errors)
		std::printf("       [err] line %d: %s\n", e.line, e.message.c_str());
	std::string s1 = MXRender::UI::UIDocModel::UIDocumentSerializer::SerializeRml(doc);
	auto doc2 = MXRender::UI::UIDocModel::RmlParser::Parse(s1, path.string());
	std::string s2 = MXRender::UI::UIDocModel::UIDocumentSerializer::SerializeRml(doc2);
	if (s1 == s2)
		std::printf("PASS  %s (errors: %zu)\n", path.filename().string().c_str(), doc.errors.size());
	else
	{
		std::printf("FAIL  %s — round-trip not idempotent\n", path.filename().string().c_str());
		++failures;
	}
}

void CheckRcss(const fs::path& path)
{
	std::string text = ReadFile(path);
	auto ss = MXRender::UI::UIDocModel::UIRcssParser::Parse(text, path.string());
	std::string s1 = MXRender::UI::UIDocModel::UIDocumentSerializer::SerializeRcss(ss);
	auto ss2 = MXRender::UI::UIDocModel::UIRcssParser::Parse(s1, path.string());
	std::string s2 = MXRender::UI::UIDocModel::UIDocumentSerializer::SerializeRcss(ss2);
	if (s1 == s2)
		std::printf("PASS  %s (errors: %zu)\n", path.filename().string().c_str(), ss.errors.size());
	else
	{
		std::printf("FAIL  %s — round-trip not idempotent\n", path.filename().string().c_str());
		++failures;
	}
}

} // namespace

int main(int argc, char** argv)
{
	if (argc < 2)
	{
		std::printf("usage: UIRoundTripTest file.rml [file.rcss ...]\n");
		return 2;
	}
	for (int i = 1; i < argc; ++i)
	{
		fs::path p(argv[i]);
		if (!fs::exists(p)) { std::printf("SKIP  %s (missing)\n", argv[i]); continue; }
		if (p.extension() == ".rcss") CheckRcss(p);
		else CheckRml(p);
	}
	std::printf("%s (%d failure%s)\n", failures == 0 ? "ALL CLEAN" : "FAILURES",
		failures, failures == 1 ? "" : "s");
	return failures == 0 ? 0 : 1;
}
