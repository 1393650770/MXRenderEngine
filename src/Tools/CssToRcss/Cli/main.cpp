// CssToRcss — convert generic CSS (from design tools like Figma) into RCSS.
//
// Usage:
//   CssToRcss input.css [output.rcss]      convert (no output → stdout)
//   CssToRcss --report out.json input.css  also write a JSON issue report
//   CssToRcss --watch input.css [out.rcss] convert on change (0.5s poll)
//   CssToRcss --self-test [dir]            byte-compare Tests/*.css vs goldens
//   CssToRcss --write-golden [dir]         (re)generate goldens
//
// The library (CssToRcssLib) is dependency-free — it is also linked by the
// Editor for property-typed editing and CSS import.

#include "CssParser.h"
#include "RcssConverter.h"
#include "RcssEmitter.h"
#include "ConversionReport.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string ReadFile(const fs::path& path, bool& ok)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
	{
		ok = false;
		return {};
	}
	ok = true;
	return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool WriteFile(const fs::path& path, const std::string& text)
{
	// Atomic write: temp file + rename.
	fs::path tmp = path;
	tmp += ".tmp";
	{
		std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
		if (!out) return false;
		out.write(text.data(), (std::streamsize)text.size());
	}
	std::error_code ec;
	fs::rename(tmp, path, ec);
	return !ec;
}

/// Convert text → RCSS text; prints issues; returns false on parse failure.
bool ConvertText(const std::string& css, std::string& out_rcss)
{
	auto stylesheet = MXRender::Tool::CssToRcss::CssParser::Parse(css);
	MXRender::Tool::CssToRcss::RcssConverter::Convert(stylesheet);
	out_rcss = MXRender::Tool::CssToRcss::RcssEmitter::Emit(stylesheet);
	MXRender::Tool::CssToRcss::ConversionReport(stylesheet).PrintToConsole();
	return true;
}

int SelfTest(const fs::path& dir, bool write_golden)
{
	std::error_code dir_ec;
	if (!fs::is_directory(dir, dir_ec))
	{
		std::printf("[CssToRcss] self-test dir not found: %s\n", dir.string().c_str());
		return 1;
	}
	int passed = 0, failed = 0;
	std::vector<fs::path> css_files;
	std::error_code ec;
	for (const auto& entry : fs::directory_iterator(dir, ec))
	{
		if (ec) break;
		if (entry.is_regular_file() && entry.path().extension() == ".css")
			css_files.push_back(entry.path());
	}
	std::sort(css_files.begin(), css_files.end());
	for (const auto& css_path : css_files)
	{
		bool ok = false;
		std::string css = ReadFile(css_path, ok);
		if (!ok) { std::printf("[CssToRcss] FAIL: cannot read %s\n", css_path.string().c_str()); ++failed; continue; }

		std::string rcss;
		ConvertText(css, rcss);

		fs::path golden = css_path;
		golden.replace_extension(".rcss");
		if (write_golden)
		{
			WriteFile(golden, rcss);
			std::printf("[CssToRcss] golden written: %s\n", golden.string().c_str());
			++passed;
			continue;
		}
		bool ok2 = false;
		std::string expected = ReadFile(golden, ok2);
		if (!ok2)
		{
			std::printf("[CssToRcss] FAIL: missing golden %s (run --write-golden)\n", golden.string().c_str());
			++failed;
			continue;
		}
		if (rcss == expected)
		{
			std::printf("[CssToRcss] PASS: %s\n", css_path.filename().string().c_str());
			++passed;
		}
		else
		{
			std::printf("[CssToRcss] FAIL: %s\n", css_path.filename().string().c_str());
			++failed;
		}
	}
	std::printf("[CssToRcss] %d passed, %d failed\n", passed, failed);
	return failed == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
	std::vector<std::string> args(argv + 1, argv + argc);
	std::string report_path;

	bool watch = false, self_test = false, write_golden = false;
	for (size_t i = 0; i < args.size(); )
	{
		if (args[i] == "--watch") { watch = true; args.erase(args.begin() + (long)i); }
		else if (args[i] == "--self-test") { self_test = true; args.erase(args.begin() + (long)i); }
		else if (args[i] == "--write-golden") { write_golden = true; args.erase(args.begin() + (long)i); }
		else if (args[i] == "--report" && i + 1 < args.size())
		{
			report_path = args[i + 1];
			args.erase(args.begin() + (long)i, args.begin() + (long)i + 2);
		}
		else ++i;
	}

	// Locate the Tests dir: explicit arg, else next to the exe.
	const fs::path exe_dir = fs::path(argv[0]).parent_path();
	const fs::path tests_dir = args.size() >= 1 && fs::is_directory(args[0])
		? fs::path(args[0]) : exe_dir / "Tests";

	if (self_test || write_golden)
		return SelfTest(tests_dir, write_golden);

	if (args.empty())
	{
		std::printf("CssToRcss — CSS → RCSS converter\n"
			"Usage: CssToRcss [--watch] [--report out.json] input.css [output.rcss]\n"
			"       CssToRcss --self-test [dir] | --write-golden [dir]\n");
		return 1;
	}

	const fs::path input = args[0];
	const fs::path output = args.size() >= 2 ? fs::path(args[1]) : fs::path();

	if (watch)
	{
		std::printf("[CssToRcss] watching %s (Ctrl+C to stop)\n", input.string().c_str());
		auto last = fs::last_write_time(input);
		for (;;)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			std::error_code ec;
			auto now = fs::last_write_time(input, ec);
			if (ec || now == last) continue;
			last = now;
			bool ok = false;
			std::string css = ReadFile(input, ok);
			if (!ok) { std::printf("[CssToRcss] cannot read %s\n", input.string().c_str()); continue; }
			std::string rcss;
			ConvertText(css, rcss);
			if (!output.empty() && WriteFile(output, rcss))
				std::printf("[CssToRcss] wrote %s — hot reload picks it up\n", output.string().c_str());
			else if (!output.empty())
				std::printf("[CssToRcss] FAIL: cannot write %s\n", output.string().c_str());
		}
	}

	bool ok = false;
	std::string css = ReadFile(input, ok);
	if (!ok)
	{
		std::printf("[CssToRcss] cannot read %s\n", input.string().c_str());
		return 1;
	}
	std::string rcss;
	ConvertText(css, rcss);

	if (!report_path.empty())
	{
		auto stylesheet = MXRender::Tool::CssToRcss::CssParser::Parse(css);
		WriteFile(report_path, MXRender::Tool::CssToRcss::ConversionReport(stylesheet).ToJson());
	}

	if (!output.empty())
	{
		if (!WriteFile(output, rcss))
		{
			std::printf("[CssToRcss] cannot write %s\n", output.string().c_str());
			return 1;
		}
		std::printf("[CssToRcss] wrote %s\n", output.string().c_str());
	}
	else
	{
		std::printf("%s", rcss.c_str());
	}
	return 0;
}
