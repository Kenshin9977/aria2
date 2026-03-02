#!/usr/bin/env python3
"""Apply std::optional refactoring to BitfieldMan.h and BitfieldMan.cc"""
import subprocess

def read_file(path):
    with open(path, 'r') as f:
        return f.read()

def write_file(path, content):
    with open(path, 'w') as f:
        f.write(content)

def apply_header():
    path = 'src/BitfieldMan.h'
    content = read_file(path)

    # 1. Add #include <optional>
    content = content.replace(
        '#include "common.h"\n\n#include <vector>',
        '#include "common.h"\n\n#include <optional>\n#include <vector>'
    )

    # 2. getFirstMissingUnusedIndex
    content = content.replace(
        '  bool getFirstMissingUnusedIndex(size_t& index) const;',
        '  std::optional<size_t> getFirstMissingUnusedIndex() const;'
    )

    # 3. getFirstMissingIndex
    content = content.replace(
        '  bool getFirstMissingIndex(size_t& index) const;',
        '  std::optional<size_t> getFirstMissingIndex() const;'
    )

    # 4. getSparseMissingUnusedIndex
    content = content.replace(
        '  bool getSparseMissingUnusedIndex(size_t& index, int32_t minSplitSize,\n'
        '                                   const unsigned char* ignoreBitfield,\n'
        '                                   size_t ignoreBitfieldLength) const;',
        '  std::optional<size_t> getSparseMissingUnusedIndex(\n'
        '      int32_t minSplitSize, const unsigned char* ignoreBitfield,\n'
        '      size_t ignoreBitfieldLength) const;'
    )

    # 5. getGeomMissingUnusedIndex
    content = content.replace(
        '  bool getGeomMissingUnusedIndex(size_t& index, int32_t minSplitSize,\n'
        '                                 const unsigned char* ignoreBitfield,\n'
        '                                 size_t ignoreBitfieldLength, double base,\n'
        '                                 size_t offsetIndex) const;',
        '  std::optional<size_t> getGeomMissingUnusedIndex(\n'
        '      int32_t minSplitSize, const unsigned char* ignoreBitfield,\n'
        '      size_t ignoreBitfieldLength, double base,\n'
        '      size_t offsetIndex) const;'
    )

    # 6. getInorderMissingUnusedIndex (3-param)
    content = content.replace(
        '  bool getInorderMissingUnusedIndex(size_t& index, int32_t minSplitSize,\n'
        '                                    const unsigned char* ignoreBitfield,\n'
        '                                    size_t ignoreBitfieldLength) const;',
        '  std::optional<size_t> getInorderMissingUnusedIndex(\n'
        '      int32_t minSplitSize, const unsigned char* ignoreBitfield,\n'
        '      size_t ignoreBitfieldLength) const;'
    )

    # 7. getInorderMissingUnusedIndex (5-param with startIndex, endIndex)
    content = content.replace(
        '  bool getInorderMissingUnusedIndex(size_t& index, size_t startIndex,\n'
        '                                    size_t endIndex, int32_t minSplitSize,\n'
        '                                    const unsigned char* ignoreBitfield,\n'
        '                                    size_t ignoreBitfieldLength) const;',
        '  std::optional<size_t> getInorderMissingUnusedIndex(\n'
        '      size_t startIndex, size_t endIndex, int32_t minSplitSize,\n'
        '      const unsigned char* ignoreBitfield,\n'
        '      size_t ignoreBitfieldLength) const;'
    )

    write_file(path, content)
    print(f"Updated {path}")

def apply_cc():
    path = 'src/BitfieldMan.cc'
    content = read_file(path)

    # 1. getFirstMissingUnusedIndex
    content = content.replace(
        'bool BitfieldMan::getFirstMissingUnusedIndex(size_t& index) const\n'
        '{\n'
        '  if (filterEnabled_) {\n'
        '    return bitfield::getFirstSetBitIndex(\n'
        '        index,\n'
        '        ~array(bitfield_) & ~array(useBitfield_) & array(filterBitfield_),\n'
        '        blocks_);\n'
        '  }\n'
        '  else {\n'
        '    return bitfield::getFirstSetBitIndex(\n'
        '        index, ~array(bitfield_) & ~array(useBitfield_), blocks_);\n'
        '  }\n'
        '}',
        'std::optional<size_t> BitfieldMan::getFirstMissingUnusedIndex() const\n'
        '{\n'
        '  if (filterEnabled_) {\n'
        '    return bitfield::getFirstSetBitIndex(\n'
        '        ~array(bitfield_) & ~array(useBitfield_) & array(filterBitfield_),\n'
        '        blocks_);\n'
        '  }\n'
        '  else {\n'
        '    return bitfield::getFirstSetBitIndex(\n'
        '        ~array(bitfield_) & ~array(useBitfield_), blocks_);\n'
        '  }\n'
        '}'
    )

    # 2. getFirstMissingIndex
    content = content.replace(
        'bool BitfieldMan::getFirstMissingIndex(size_t& index) const\n'
        '{\n'
        '  if (filterEnabled_) {\n'
        '    return bitfield::getFirstSetBitIndex(\n'
        '        index, ~array(bitfield_) & array(filterBitfield_), blocks_);\n'
        '  }\n'
        '  else {\n'
        '    return bitfield::getFirstSetBitIndex(index, ~array(bitfield_), blocks_);\n'
        '  }\n'
        '}',
        'std::optional<size_t> BitfieldMan::getFirstMissingIndex() const\n'
        '{\n'
        '  if (filterEnabled_) {\n'
        '    return bitfield::getFirstSetBitIndex(\n'
        '        ~array(bitfield_) & array(filterBitfield_), blocks_);\n'
        '  }\n'
        '  else {\n'
        '    return bitfield::getFirstSetBitIndex(~array(bitfield_), blocks_);\n'
        '  }\n'
        '}'
    )

    # 3. Anonymous namespace getSparseMissingUnusedIndex template
    content = content.replace(
        'template <typename Array>\n'
        'bool getSparseMissingUnusedIndex(size_t& index, int32_t minSplitSize,\n'
        '                                 const Array& bitfield,\n'
        '                                 const unsigned char* useBitfield,\n'
        '                                 int32_t blockLength, size_t blocks)\n'
        '{\n'
        '  BitfieldMan::Range maxRange;\n'
        '  BitfieldMan::Range currentRange;\n'
        '  size_t nextIndex = 0;',
        'template <typename Array>\n'
        'std::optional<size_t>\n'
        'getSparseMissingUnusedIndex(int32_t minSplitSize,\n'
        '                            const Array& bitfield,\n'
        '                            const unsigned char* useBitfield,\n'
        '                            int32_t blockLength, size_t blocks)\n'
        '{\n'
        '  BitfieldMan::Range maxRange;\n'
        '  BitfieldMan::Range currentRange;\n'
        '  size_t nextIndex = 0;'
    )

    # Fix the return statements in sparse template
    content = content.replace(
        '    if (maxRange.startIndex == 0) {\n'
        '      index = 0;\n'
        '      return true;\n'
        '    }\n'
        '    else {\n'
        '      if ((!bitfield::test(useBitfield, blocks, maxRange.startIndex - 1) &&\n'
        '           bitfield::test(bitfield, blocks, maxRange.startIndex - 1)) ||\n'
        '          (static_cast<int64_t>(maxRange.endIndex - maxRange.startIndex) *\n'
        '               blockLength >=\n'
        '           minSplitSize)) {\n'
        '        index = maxRange.startIndex;\n'
        '        return true;\n'
        '      }\n'
        '      else {\n'
        '        return false;\n'
        '      }\n'
        '    }\n'
        '  }\n'
        '  else {\n'
        '    return false;\n'
        '  }\n'
        '}\n'
        '} // namespace\n'
        '\n'
        'bool BitfieldMan::getSparseMissingUnusedIndex(\n'
        '    size_t& index, int32_t minSplitSize, const unsigned char* ignoreBitfield,\n'
        '    size_t ignoreBitfieldLength) const\n'
        '{\n'
        '  if (filterEnabled_) {\n'
        '    return aria2::getSparseMissingUnusedIndex(\n'
        '        index, minSplitSize,',
        '    if (maxRange.startIndex == 0) {\n'
        '      return size_t{0};\n'
        '    }\n'
        '    else {\n'
        '      if ((!bitfield::test(useBitfield, blocks, maxRange.startIndex - 1) &&\n'
        '           bitfield::test(bitfield, blocks, maxRange.startIndex - 1)) ||\n'
        '          (static_cast<int64_t>(maxRange.endIndex - maxRange.startIndex) *\n'
        '               blockLength >=\n'
        '           minSplitSize)) {\n'
        '        return maxRange.startIndex;\n'
        '      }\n'
        '      else {\n'
        '        return std::nullopt;\n'
        '      }\n'
        '    }\n'
        '  }\n'
        '  else {\n'
        '    return std::nullopt;\n'
        '  }\n'
        '}\n'
        '} // namespace\n'
        '\n'
        'std::optional<size_t> BitfieldMan::getSparseMissingUnusedIndex(\n'
        '    int32_t minSplitSize, const unsigned char* ignoreBitfield,\n'
        '    size_t ignoreBitfieldLength) const\n'
        '{\n'
        '  if (filterEnabled_) {\n'
        '    return aria2::getSparseMissingUnusedIndex(\n'
        '        minSplitSize,'
    )

    # Fix the rest of public sparse method
    content = content.replace(
        '        useBitfield_, blockLength_, blocks_);\n'
        '  }\n'
        '  else {\n'
        '    return aria2::getSparseMissingUnusedIndex(\n'
        '        index, minSplitSize,\n'
        '        array(ignoreBitfield) | array(bitfield_) | array(useBitfield_),\n'
        '        useBitfield_, blockLength_, blocks_);\n'
        '  }\n'
        '}\n'
        '\n'
        'namespace {\n'
        'template <typename Array>\n'
        'bool getGeomMissingUnusedIndex(size_t& index, int32_t minSplitSize,',
        '        useBitfield_, blockLength_, blocks_);\n'
        '  }\n'
        '  else {\n'
        '    return aria2::getSparseMissingUnusedIndex(\n'
        '        minSplitSize,\n'
        '        array(ignoreBitfield) | array(bitfield_) | array(useBitfield_),\n'
        '        useBitfield_, blockLength_, blocks_);\n'
        '  }\n'
        '}\n'
        '\n'
        'namespace {\n'
        'template <typename Array>\n'
        'std::optional<size_t>\n'
        'getGeomMissingUnusedIndex(int32_t minSplitSize,'
    )

    # 4. Anonymous namespace getGeomMissingUnusedIndex template
    content = content.replace(
        'getGeomMissingUnusedIndex(int32_t minSplitSize,\n'
        '                               const Array& bitfield,\n'
        '                               const unsigned char* useBitfield,\n'
        '                               int32_t blockLength, size_t blocks, double base,\n'
        '                               size_t offsetIndex)\n'
        '{\n'
        '  double start = 0;\n'
        '  double end = 1;\n'
        '  while (start + offsetIndex < blocks) {\n'
        '    index = blocks;',
        'getGeomMissingUnusedIndex(int32_t minSplitSize,\n'
        '                          const Array& bitfield,\n'
        '                          const unsigned char* useBitfield,\n'
        '                          int32_t blockLength, size_t blocks, double base,\n'
        '                          size_t offsetIndex)\n'
        '{\n'
        '  double start = 0;\n'
        '  double end = 1;\n'
        '  while (start + offsetIndex < blocks) {\n'
        '    size_t found = blocks;'
    )

    # Fix geom template body: index -> found
    content = content.replace(
        '        index = i;\n'
        '        break;\n'
        '      }\n'
        '    }\n'
        '    if (index < blocks) {\n'
        '      return true;\n'
        '    }',
        '        found = i;\n'
        '        break;\n'
        '      }\n'
        '    }\n'
        '    if (found < blocks) {\n'
        '      return found;\n'
        '    }'
    )

    # Fix geom template fallback
    content = content.replace(
        '  return getSparseMissingUnusedIndex(index, minSplitSize, bitfield, useBitfield,\n'
        '                                     blockLength, blocks);',
        '  return getSparseMissingUnusedIndex(minSplitSize, bitfield, useBitfield,\n'
        '                                     blockLength, blocks);'
    )

    # 5. Public getGeomMissingUnusedIndex
    content = content.replace(
        'bool BitfieldMan::getGeomMissingUnusedIndex(size_t& index, int32_t minSplitSize,\n'
        '                                            const unsigned char* ignoreBitfield,\n'
        '                                            size_t ignoreBitfieldLength,\n'
        '                                            double base,\n'
        '                                            size_t offsetIndex) const\n'
        '{\n'
        '  if (filterEnabled_) {\n'
        '    return aria2::getGeomMissingUnusedIndex(\n'
        '        index, minSplitSize,',
        'std::optional<size_t> BitfieldMan::getGeomMissingUnusedIndex(\n'
        '    int32_t minSplitSize, const unsigned char* ignoreBitfield,\n'
        '    size_t ignoreBitfieldLength, double base, size_t offsetIndex) const\n'
        '{\n'
        '  if (filterEnabled_) {\n'
        '    return aria2::getGeomMissingUnusedIndex(\n'
        '        minSplitSize,'
    )

    # Fix the rest of public geom method (else branch)
    content = content.replace(
        '        useBitfield_, blockLength_, blocks_, base, offsetIndex);\n'
        '  }\n'
        '  else {\n'
        '    return aria2::getGeomMissingUnusedIndex(\n'
        '        index, minSplitSize,\n'
        '        array(ignoreBitfield) | array(bitfield_) | array(useBitfield_),\n'
        '        useBitfield_, blockLength_, blocks_, base, offsetIndex);\n'
        '  }\n'
        '}\n'
        '\n'
        'namespace {\n'
        'template <typename Array>\n'
        'bool getInorderMissingUnusedIndex(size_t& index, size_t startIndex,',
        '        useBitfield_, blockLength_, blocks_, base, offsetIndex);\n'
        '  }\n'
        '  else {\n'
        '    return aria2::getGeomMissingUnusedIndex(\n'
        '        minSplitSize,\n'
        '        array(ignoreBitfield) | array(bitfield_) | array(useBitfield_),\n'
        '        useBitfield_, blockLength_, blocks_, base, offsetIndex);\n'
        '  }\n'
        '}\n'
        '\n'
        'namespace {\n'
        'template <typename Array>\n'
        'std::optional<size_t>\n'
        'getInorderMissingUnusedIndex(size_t startIndex,'
    )

    # 6. Anonymous namespace getInorderMissingUnusedIndex template
    content = content.replace(
        'getInorderMissingUnusedIndex(size_t startIndex,\n'
        '                                  size_t lastIndex, int32_t minSplitSize,\n'
        '                                  const Array& bitfield,\n'
        '                                  const unsigned char* useBitfield,\n'
        '                                  int32_t blockLength, size_t blocks)',
        'getInorderMissingUnusedIndex(size_t startIndex,\n'
        '                             size_t lastIndex, int32_t minSplitSize,\n'
        '                             const Array& bitfield,\n'
        '                             const unsigned char* useBitfield,\n'
        '                             int32_t blockLength, size_t blocks)'
    )

    # Fix inorder template: first piece return
    content = content.replace(
        '  if (!bitfield::test(bitfield, blocks, startIndex) &&\n'
        '      !bitfield::test(useBitfield, blocks, startIndex)) {\n'
        '    index = startIndex;\n'
        '    return true;\n'
        '  }',
        '  if (!bitfield::test(bitfield, blocks, startIndex) &&\n'
        '      !bitfield::test(useBitfield, blocks, startIndex)) {\n'
        '    return startIndex;\n'
        '  }'
    )

    # Fix inorder template: previous piece retrieved return
    content = content.replace(
        '        index = i;\n'
        '        return true;\n'
        '      }\n'
        '      // Check free space of minSplitSize.',
        '        return i;\n'
        '      }\n'
        '      // Check free space of minSplitSize.'
    )

    # Fix inorder template: minSplitSize return
    content = content.replace(
        '        if (static_cast<int64_t>(j - i + 1) * blockLength >= minSplitSize) {\n'
        '          index = j;\n'
        '          return true;\n'
        '        }',
        '        if (static_cast<int64_t>(j - i + 1) * blockLength >= minSplitSize) {\n'
        '          return j;\n'
        '        }'
    )

    # Fix inorder template: final return false
    content = content.replace(
        '  return false;\n'
        '}\n'
        '} // namespace\n'
        '\n'
        'bool BitfieldMan::getInorderMissingUnusedIndex(\n'
        '    size_t& index, int32_t minSplitSize, const unsigned char* ignoreBitfield,\n'
        '    size_t ignoreBitfieldLength) const\n'
        '{\n'
        '  if (filterEnabled_) {\n'
        '    return aria2::getInorderMissingUnusedIndex(\n'
        '        index, 0, blocks_, minSplitSize,',
        '  return std::nullopt;\n'
        '}\n'
        '} // namespace\n'
        '\n'
        'std::optional<size_t> BitfieldMan::getInorderMissingUnusedIndex(\n'
        '    int32_t minSplitSize, const unsigned char* ignoreBitfield,\n'
        '    size_t ignoreBitfieldLength) const\n'
        '{\n'
        '  if (filterEnabled_) {\n'
        '    return aria2::getInorderMissingUnusedIndex(\n'
        '        0, blocks_, minSplitSize,'
    )

    # Fix first public inorder method (else branch)
    content = content.replace(
        '  else {\n'
        '    return aria2::getInorderMissingUnusedIndex(\n'
        '        index, 0, blocks_, minSplitSize,\n'
        '        array(ignoreBitfield) | array(bitfield_) | array(useBitfield_),',
        '  else {\n'
        '    return aria2::getInorderMissingUnusedIndex(\n'
        '        0, blocks_, minSplitSize,\n'
        '        array(ignoreBitfield) | array(bitfield_) | array(useBitfield_),'
    )

    # 7. Second public getInorderMissingUnusedIndex (with startIndex, endIndex)
    content = content.replace(
        'bool BitfieldMan::getInorderMissingUnusedIndex(\n'
        '    size_t& index, size_t startIndex, size_t endIndex, int32_t minSplitSize,\n'
        '    const unsigned char* ignoreBitfield, size_t ignoreBitfieldLength) const\n'
        '{\n'
        '  endIndex = std::min(endIndex, blocks_);\n'
        '  if (filterEnabled_) {\n'
        '    return aria2::getInorderMissingUnusedIndex(\n'
        '        index, startIndex, endIndex, minSplitSize,',
        'std::optional<size_t> BitfieldMan::getInorderMissingUnusedIndex(\n'
        '    size_t startIndex, size_t endIndex, int32_t minSplitSize,\n'
        '    const unsigned char* ignoreBitfield, size_t ignoreBitfieldLength) const\n'
        '{\n'
        '  endIndex = std::min(endIndex, blocks_);\n'
        '  if (filterEnabled_) {\n'
        '    return aria2::getInorderMissingUnusedIndex(\n'
        '        startIndex, endIndex, minSplitSize,'
    )

    # Fix second public inorder method (else branch)
    content = content.replace(
        '  else {\n'
        '    return aria2::getInorderMissingUnusedIndex(\n'
        '        index, startIndex, endIndex, minSplitSize,\n'
        '        array(ignoreBitfield) | array(bitfield_) | array(useBitfield_),',
        '  else {\n'
        '    return aria2::getInorderMissingUnusedIndex(\n'
        '        startIndex, endIndex, minSplitSize,\n'
        '        array(ignoreBitfield) | array(bitfield_) | array(useBitfield_),'
    )

    write_file(path, content)
    print(f"Updated {path}")

if __name__ == '__main__':
    apply_header()
    apply_cc()
    # Immediately save a patch
    result = subprocess.run(
        ['git', 'diff', 'src/BitfieldMan.h', 'src/BitfieldMan.cc'],
        capture_output=True, text=True
    )
    with open('.raw/bitfieldman.patch', 'w') as f:
        f.write(result.stdout)
    print(f"Saved patch ({len(result.stdout)} bytes)")
