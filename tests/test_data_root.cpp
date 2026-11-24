// SPDX-License-Identifier: GPL-3.0-or-later
#include "zen1/data_root.hpp"
#include "zen1/version.hpp"
#include "zen1_test.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

using namespace zen1;
namespace fs = std::filesystem;

namespace {
fs::path tempRoot(const char* tag) {
    return fs::temp_directory_path() /
           (std::string("zen1-root-") + tag + "-" + std::to_string(::getpid())) / "Zen1Arcade";
}
std::string slurp(const fs::path& p) {
    std::ifstream in(p);
    std::stringstream s;
    s << in.rdbuf();
    return s.str();
}
}  // namespace

TEST("First launch creates the full folder layout, README and marker") {
    const auto root = tempRoot("create");
    fs::remove_all(root.parent_path());
    const auto r = ensureDataRoot(root);
    CHECK(r.ok);
    for (const auto& rel : dataFolderLayout()) CHECK(fs::is_directory(root / rel));
    CHECK(fs::exists(root / "README.txt"));
    CHECK_EQ(r.formatVersion, kDataFormatVersion);
    fs::remove_all(root.parent_path());
}

TEST("Existing games, saves and edited files are never touched") {
    const auto root = tempRoot("keep");
    fs::remove_all(root.parent_path());
    CHECK(ensureDataRoot(root).ok);
    std::ofstream(root / "Games" / "sf2.zip") << "game-bytes";
    std::ofstream(root / "Saves" / "sf2.nv") << "save-bytes";
    std::ofstream(root / "README.txt", std::ios::trunc) << "my own notes";

    // Simulate an update: run again.
    const auto r = ensureDataRoot(root);
    CHECK(r.ok);
    CHECK(r.created.empty());
    CHECK_EQ(slurp(root / "Games" / "sf2.zip"), std::string("game-bytes"));
    CHECK_EQ(slurp(root / "Saves" / "sf2.nv"), std::string("save-bytes"));
    CHECK_EQ(slurp(root / "README.txt"), std::string("my own notes"));
    fs::remove_all(root.parent_path());
}

TEST("A missing sub-folder is recreated without affecting others") {
    const auto root = tempRoot("repair");
    fs::remove_all(root.parent_path());
    CHECK(ensureDataRoot(root).ok);
    fs::remove_all(root / "Artwork" / "bezels");
    const auto r = ensureDataRoot(root);
    CHECK(r.ok);
    CHECK_EQ(r.created.size(), static_cast<size_t>(1));
    CHECK(fs::is_directory(root / "Artwork" / "bezels"));
    fs::remove_all(root.parent_path());
}

TEST("A newer data format is refused, never modified") {
    const auto root = tempRoot("newer");
    fs::remove_all(root.parent_path());
    fs::create_directories(root);
    std::ofstream(root / ".zen1-data-version") << (kDataFormatVersion + 1) << "\n";
    const auto r = ensureDataRoot(root);
    CHECK(!r.ok);
    CHECK(!fs::exists(root / "Games"));
    fs::remove_all(root.parent_path());
}

TEST("A file where the data folder should be is reported, not replaced") {
    const auto root = tempRoot("file");
    fs::remove_all(root.parent_path());
    fs::create_directories(root.parent_path());
    std::ofstream(root) << "not a folder";
    const auto r = ensureDataRoot(root);
    CHECK(!r.ok);
    CHECK_EQ(slurp(root), std::string("not a folder"));
    fs::remove_all(root.parent_path());
}
