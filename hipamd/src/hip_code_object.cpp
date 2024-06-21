/*
Copyright (c) 2015 - 2021 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#include "hip_code_object.hpp"
#include "amd_hsa_elf.hpp"

#include <cstring>

#include <hip/driver_types.h>
#include "hip/hip_runtime_api.h"
#include "hip/hip_runtime.h"
#include "hip_internal.hpp"
#include "platform/program.hpp"
#include <elf/elf.hpp>
namespace hip {
hipError_t ihipFree(void* ptr);
// forward declaration of methods required for managed variables
hipError_t ihipMallocManaged(void** ptr, size_t size, unsigned int align = 0);
namespace {
constexpr char kOffloadBundleMagicStr[] = "__CLANG_OFFLOAD_BUNDLE__";
constexpr char kOffloadKindHip[] = "hip";
constexpr char kOffloadKindHipv4[] = "hipv4";
constexpr char kOffloadKindHcc[] = "hcc";
constexpr char kAmdgcnTargetTriple[] = "amdgcn-amd-amdhsa-";

// ClangOFFLOADBundle info.
static constexpr size_t kOffloadBundleMagicStrSize = sizeof(kOffloadBundleMagicStr);

// Clang Offload bundler description & Header.
struct __ClangOffloadBundleInfo {
  uint64_t offset;
  uint64_t size;
  uint64_t bundleEntryIdSize;
  const char bundleEntryId[1];
};

struct __ClangOffloadBundleHeader {
  const char magic[kOffloadBundleMagicStrSize - 1];
  uint64_t numOfCodeObjects;
  __ClangOffloadBundleInfo desc[1];
};
}  // namespace

bool CodeObject::IsClangOffloadMagicBundle(const void* data) {
  std::string magic(reinterpret_cast<const char*>(data), kOffloadBundleMagicStrSize - 1);
  return magic.compare(kOffloadBundleMagicStr) ? false : true;
}

uint64_t CodeObject::ElfSize(const void* emi) { return amd::Elf::getElfSize(emi); }

static bool getProcName(uint32_t EFlags, std::string& proc_name, bool& xnackSupported,
                        bool& sramEccSupported) {
  switch (EFlags & EF_AMDGPU_MACH) {
    case EF_AMDGPU_MACH_AMDGCN_GFX700:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx700";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX701:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx701";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX702:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx702";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX703:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx703";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX704:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx704";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX705:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx705";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX801:
      xnackSupported = true;
      sramEccSupported = false;
      proc_name = "gfx801";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX802:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx802";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX803:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx803";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX805:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx805";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX810:
      xnackSupported = true;
      sramEccSupported = false;
      proc_name = "gfx810";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX900:
      xnackSupported = true;
      sramEccSupported = false;
      proc_name = "gfx900";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX902:
      xnackSupported = true;
      sramEccSupported = false;
      proc_name = "gfx902";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX904:
      xnackSupported = true;
      sramEccSupported = false;
      proc_name = "gfx904";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX906:
      xnackSupported = true;
      sramEccSupported = true;
      proc_name = "gfx906";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX908:
      xnackSupported = true;
      sramEccSupported = true;
      proc_name = "gfx908";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX909:
      xnackSupported = true;
      sramEccSupported = false;
      proc_name = "gfx909";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX90A:
      xnackSupported = true;
      sramEccSupported = true;
      proc_name = "gfx90a";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX90C:
      xnackSupported = true;
      sramEccSupported = false;
      proc_name = "gfx90c";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX940:
      xnackSupported = true;
      sramEccSupported = true;
      proc_name = "gfx940";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX941:
      xnackSupported = true;
      sramEccSupported = true;
      proc_name = "gfx941";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX942:
      xnackSupported = true;
      sramEccSupported = true;
      proc_name = "gfx942";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1010:
      xnackSupported = true;
      sramEccSupported = false;
      proc_name = "gfx1010";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1011:
      xnackSupported = true;
      sramEccSupported = false;
      proc_name = "gfx1011";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1012:
      xnackSupported = true;
      sramEccSupported = false;
      proc_name = "gfx1012";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1013:
      xnackSupported = true;
      sramEccSupported = false;
      proc_name = "gfx1013";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1030:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1030";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1031:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1031";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1032:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1032";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1033:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1033";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1034:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1034";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1035:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1035";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1036:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1036";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1100:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1100";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1101:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1101";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1102:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1102";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1103:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1103";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1150:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1150";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1151:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1151";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1200:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1200";
      break;
    case EF_AMDGPU_MACH_AMDGCN_GFX1201:
      xnackSupported = false;
      sramEccSupported = false;
      proc_name = "gfx1201";
      break;
    default:
      return false;
  }
  return true;
}

static bool getTripleTargetIDFromCodeObject(const void* code_object, std::string& target_id) {
  if (!code_object) return false;
  const Elf64_Ehdr* ehdr = reinterpret_cast<const Elf64_Ehdr*>(code_object);
  if (ehdr->e_machine != EM_AMDGPU) return false;
  if (ehdr->e_ident[EI_OSABI] != ELFOSABI_AMDGPU_HSA) return false;

  bool isXnackSupported{false}, isSramEccSupported{false};

  std::string proc_name;
  if (!getProcName(ehdr->e_flags, proc_name, isXnackSupported, isSramEccSupported)) return false;
  target_id = std::string(kAmdgcnTargetTriple) + '-' + proc_name;

  switch (ehdr->e_ident[EI_ABIVERSION]) {
    case ELFABIVERSION_AMDGPU_HSA_V2: {
      LogPrintfInfo("[Code Object V2, target id:%s]", target_id.c_str());
      return false;
    }

    case ELFABIVERSION_AMDGPU_HSA_V3: {
      LogPrintfInfo("[Code Object V3, target id:%s]", target_id.c_str());
      if (isSramEccSupported) {
        if (ehdr->e_flags & EF_AMDGPU_FEATURE_SRAMECC_V3)
          target_id += ":sramecc+";
        else
          target_id += ":sramecc-";
      }
      if (isXnackSupported) {
        if (ehdr->e_flags & EF_AMDGPU_FEATURE_XNACK_V3)
          target_id += ":xnack+";
        else
          target_id += ":xnack-";
      }
      break;
    }

    case ELFABIVERSION_AMDGPU_HSA_V4:
    case ELFABIVERSION_AMDGPU_HSA_V5: {
      if (ehdr->e_ident[EI_ABIVERSION] & ELFABIVERSION_AMDGPU_HSA_V4) {
        LogPrintfInfo("[Code Object V4, target id:%s]", target_id.c_str());
      } else {
        LogPrintfInfo("[Code Object V5, target id:%s]", target_id.c_str());
      }
      unsigned co_sram_value = (ehdr->e_flags) & EF_AMDGPU_FEATURE_SRAMECC_V4;
      if (co_sram_value == EF_AMDGPU_FEATURE_SRAMECC_OFF_V4)
        target_id += ":sramecc-";
      else if (co_sram_value == EF_AMDGPU_FEATURE_SRAMECC_ON_V4)
        target_id += ":sramecc+";

      unsigned co_xnack_value = (ehdr->e_flags) & EF_AMDGPU_FEATURE_XNACK_V4;
      if (co_xnack_value == EF_AMDGPU_FEATURE_XNACK_OFF_V4)
        target_id += ":xnack-";
      else if (co_xnack_value == EF_AMDGPU_FEATURE_XNACK_ON_V4)
        target_id += ":xnack+";
      break;
    }

    default: {
      return false;
    }
  }
  return true;
}

// Consumes the string 'consume_' from the starting of the given input
// eg: input = amdgcn-amd-amdhsa--gfx908 and consume_ is amdgcn-amd-amdhsa--
// input will become gfx908.
static bool consume(std::string& input, std::string consume_) {
  if (input.substr(0, consume_.size()) != consume_) {
    return false;
  }
  input = input.substr(consume_.size());
  return true;
}

// Trim String till character, will be used to get gpuname
// example: input is gfx908:sram-ecc+ and trim char is :
// input will become sram-ecc+.
static std::string trimName(std::string& input, char trim) {
  auto pos_ = input.find(trim);
  auto res = input;
  if (pos_ == std::string::npos) {
    input = "";
  } else {
    res = input.substr(0, pos_);
    input = input.substr(pos_);
  }
  return res;
}

static char getFeatureValue(std::string& input, std::string feature) {
  char res = ' ';
  if (consume(input, std::move(feature))) {
    res = input[0];
    input = input.substr(1);
  }
  return res;
}

static bool getTargetIDValue(std::string& input, std::string& processor, char& sramecc_value,
                             char& xnack_value) {
  processor = trimName(input, ':');
  sramecc_value = getFeatureValue(input, std::string(":sramecc"));
  if (sramecc_value != ' ' && sramecc_value != '+' && sramecc_value != '-') return false;
  xnack_value = getFeatureValue(input, std::string(":xnack"));
  if (xnack_value != ' ' && xnack_value != '+' && xnack_value != '-') return false;
  return true;
}

static bool getTripleTargetID(std::string bundled_co_entry_id, const void* code_object,
                              std::string& co_triple_target_id) {
  std::string offload_kind = trimName(bundled_co_entry_id, '-');
  if (offload_kind != kOffloadKindHipv4 && offload_kind != kOffloadKindHip &&
      offload_kind != kOffloadKindHcc)
    return false;

  if (offload_kind != kOffloadKindHipv4)
    return getTripleTargetIDFromCodeObject(code_object, co_triple_target_id);

  // For code object V4 onwards the bundled code object entry ID correctly
  // specifies the target triple.
  co_triple_target_id = bundled_co_entry_id.substr(1);
  return true;
}

struct GfxPattern {
  std::string root;
  std::string suffixes;
};

static bool matches(const GfxPattern& p, const std::string& s) {
  if (p.root.size() + 1 != s.size()) {
    return false;
  }
  if (0 != std::memcmp(p.root.data(), s.data(), p.root.size())) {
    return false;
  }
  return p.suffixes.find(s[p.root.size()]) != std::string::npos;
}

static bool isGfx900EquivalentProcessor(const std::string& processor) {
  return matches(GfxPattern{"gfx90", "029c"}, processor);
}

static bool isGfx900SupersetProcessor(const std::string& processor) {
  return matches(GfxPattern{"gfx90", "0269c"}, processor);
}

static bool isGfx1030EquivalentProcessor(const std::string& processor) {
  return matches(GfxPattern{"gfx103", "0123456"}, processor);
}

static bool isGfx1010EquivalentProcessor(const std::string& processor) {
  return matches(GfxPattern{"gfx101", "0"}, processor);
}

static bool isGfx1010SupersetProcessor(const std::string& processor) {
  return matches(GfxPattern{"gfx101", "0123"}, processor);
}

enum CompatibilityScore {
  CS_EXACT_MATCH           = 1 << 4,
  CS_PROCESSOR_MATCH       = 1 << 3,
  CS_PROCESSOR_COMPATIBLE  = 1 << 2,
  CS_XNACK_SPECIALIZED     = 1 << 1,
  CS_SRAM_ECC_SPECIALIZED  = 1 << 0,
  CS_INCOMPATIBLE          = 0,
};

static int getProcessorCompatibilityScore(const std::string& co_processor,
                                          const std::string& agent_processor) {
  if (co_processor == agent_processor)
    return CS_PROCESSOR_MATCH;

  if (isGfx900SupersetProcessor(agent_processor))
    return isGfx900EquivalentProcessor(co_processor) ? CS_PROCESSOR_COMPATIBLE : CS_INCOMPATIBLE;

  if (isGfx1010SupersetProcessor(agent_processor))
    return isGfx1010EquivalentProcessor(co_processor) ? CS_PROCESSOR_COMPATIBLE : CS_INCOMPATIBLE;

  if (isGfx1030EquivalentProcessor(agent_processor))
    return isGfx1030EquivalentProcessor(co_processor) ? CS_PROCESSOR_COMPATIBLE : CS_INCOMPATIBLE;

  return CS_INCOMPATIBLE;
}

static int getCompatiblityScore(std::string co_triple_target_id,
                                std::string agent_triple_target_id) {
  // Primitive Check
  if (co_triple_target_id == agent_triple_target_id) return CS_EXACT_MATCH;

  // Parse code object triple target id
  if (!consume(co_triple_target_id, std::string(kAmdgcnTargetTriple) + '-')) {
    return CS_INCOMPATIBLE;
  }

  std::string co_processor;
  char co_sram_ecc, co_xnack;
  if (!getTargetIDValue(co_triple_target_id, co_processor, co_sram_ecc, co_xnack)) {
    return CS_INCOMPATIBLE;
  }

  if (!co_triple_target_id.empty()) return CS_INCOMPATIBLE;

  // Parse agent isa triple target id
  if (!consume(agent_triple_target_id, std::string(kAmdgcnTargetTriple) + '-')) {
    return CS_INCOMPATIBLE;
  }

  std::string agent_isa_processor;
  char isa_sram_ecc, isa_xnack;
  if (!getTargetIDValue(agent_triple_target_id, agent_isa_processor, isa_sram_ecc, isa_xnack)) {
    return CS_INCOMPATIBLE;
  }

  if (!agent_triple_target_id.empty()) return CS_INCOMPATIBLE;

  // Check for compatibility
  int processor_score = getProcessorCompatibilityScore(co_processor, agent_isa_processor);
  if (processor_score == CS_INCOMPATIBLE) {
    return CS_INCOMPATIBLE;
  }

  int xnack_bonus;
  if (co_xnack == ' ') {
    xnack_bonus = 0;
  } else if (co_xnack == isa_xnack) {
    xnack_bonus = CS_XNACK_SPECIALIZED;
  } else {
    return CS_INCOMPATIBLE;
  }

  int sram_ecc_bonus;
  if (co_sram_ecc == ' ') {
    sram_ecc_bonus = 0;
  } else if (co_sram_ecc == isa_sram_ecc) {
    sram_ecc_bonus = CS_SRAM_ECC_SPECIALIZED;
  } else {
    return CS_INCOMPATIBLE;
  }

  return processor_score + xnack_bonus + sram_ecc_bonus;
}

// This will be moved to COMGR eventually
hipError_t CodeObject::ExtractCodeObjectFromFile(
    amd::Os::FileDesc fdesc, size_t fsize, const void** image,
    const std::vector<std::string>& device_names,
    std::vector<std::pair<const void*, size_t>>& code_objs) {
  if (!amd::Os::isValidFileDesc(fdesc)) {
    return hipErrorFileNotFound;
  }

  // Map the file to memory, with offset 0.
  // file will be unmapped in ModuleUnload
  // const void* image = nullptr;
  if (!amd::Os::MemoryMapFileDesc(fdesc, fsize, 0, image)) {
    return hipErrorInvalidValue;
  }

  // retrieve code_objs{binary_image, binary_size} for devices
  return extractCodeObjectFromFatBinary(*image, device_names, code_objs);
}

// This will be moved to COMGR eventually
hipError_t CodeObject::ExtractCodeObjectFromMemory(
    const void* data, const std::vector<std::string>& device_names,
    std::vector<std::pair<const void*, size_t>>& code_objs, std::string& uri) {
  // Get the URI from memory
  if (!amd::Os::GetURIFromMemory(data, 0, uri)) {
    return hipErrorInvalidValue;
  }

  return extractCodeObjectFromFatBinary(data, device_names, code_objs);
}

// This will be moved to COMGR eventually
hipError_t CodeObject::extractCodeObjectFromFatBinary(
    const void* data, const std::vector<std::string>& agent_triple_target_ids,
    std::vector<std::pair<const void*, size_t>>& code_objs) {
  std::string magic((const char*)data, kOffloadBundleMagicStrSize);
  if (magic.compare(kOffloadBundleMagicStr)) {
    return hipErrorInvalidKernelFile;
  }

  // Initialize Code objects
  code_objs.reserve(agent_triple_target_ids.size());
  for (size_t i = 0; i < agent_triple_target_ids.size(); i++) {
    code_objs.push_back(std::make_pair(nullptr, 0));
  }
  std::vector<int> compatibilty_score(agent_triple_target_ids.size());

  const auto obheader = reinterpret_cast<const __ClangOffloadBundleHeader*>(data);
  const auto* desc = &obheader->desc[0];
  size_t num_code_objs = code_objs.size();
  for (uint64_t i = 0; i < obheader->numOfCodeObjects; ++i,
                desc = reinterpret_cast<const __ClangOffloadBundleInfo*>(
                    reinterpret_cast<uintptr_t>(&desc->bundleEntryId[0]) +
                    desc->bundleEntryIdSize)) {
    const void* image =
        reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(obheader) + desc->offset);
    const size_t image_size = desc->size;

    std::string bundleEntryId{desc->bundleEntryId, desc->bundleEntryIdSize};

    std::string co_triple_target_id;
    if (!getTripleTargetID(bundleEntryId, image, co_triple_target_id)) continue;

    for (size_t dev = 0; dev < agent_triple_target_ids.size(); ++dev) {
      int score = getCompatiblityScore(co_triple_target_id, agent_triple_target_ids[dev]);
      if (score > compatibilty_score[dev]) {
        compatibilty_score[dev] = score;
        if (!code_objs[dev].first)
          --num_code_objs;
        code_objs[dev] = std::make_pair(image, image_size);
      }
    }
  }
  if (num_code_objs == 0) {
    return hipSuccess;
  } else {
    LogPrintfError("%s",
                   "hipErrorNoBinaryForGpu: Unable to find code object for all current devices!");
    LogPrintfError("%s", "  Devices:");
    for (size_t i = 0; i < agent_triple_target_ids.size(); i++) {
      LogPrintfError("    %s - [%s]", agent_triple_target_ids[i].c_str(),
                     ((code_objs[i].first) ? "Found" : "Not Found"));
    }
    const auto obheader = reinterpret_cast<const __ClangOffloadBundleHeader*>(data);
    const auto* desc = &obheader->desc[0];
    LogPrintfError("%s", "  Bundled Code Objects:");
    for (uint64_t i = 0; i < obheader->numOfCodeObjects; ++i,
                  desc = reinterpret_cast<const __ClangOffloadBundleInfo*>(
                      reinterpret_cast<uintptr_t>(&desc->bundleEntryId[0]) +
                      desc->bundleEntryIdSize)) {
      std::string bundleEntryId{desc->bundleEntryId, desc->bundleEntryIdSize};
      const void* image =
          reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(obheader) + desc->offset);

      std::string co_triple_target_id;
      bool valid_co = getTripleTargetID(bundleEntryId, image, co_triple_target_id);

      if (valid_co) {
        LogPrintfError("    %s - [Code object targetID is %s]", bundleEntryId.c_str(),
                       co_triple_target_id.c_str());
      } else {
        LogPrintfError("    %s - [Unsupported]", bundleEntryId.c_str());
      }
    }
    return hipErrorNoBinaryForGpu;
  }
}

hipError_t DynCO::loadCodeObject(const char* fname, const void* image) {
  amd::ScopedLock lock(dclock_);

  // Number of devices = 1 in dynamic code object
  fb_info_ = new FatBinaryInfo(fname, image);
  std::vector<hip::Device*> devices = {g_devices[ihipGetDevice()]};
  IHIP_RETURN_ONFAIL(fb_info_->ExtractFatBinary(devices));

  // No Lazy loading for DynCO
  IHIP_RETURN_ONFAIL(fb_info_->BuildProgram(ihipGetDevice()));

  // Define Global variables
  IHIP_RETURN_ONFAIL(populateDynGlobalVars());

  // Define Global functions
  IHIP_RETURN_ONFAIL(populateDynGlobalFuncs());

  return hipSuccess;
}

// Dynamic Code Object
DynCO::~DynCO() {
  amd::ScopedLock lock(dclock_);

  for (auto& elem : vars_) {
    if (elem.second->getVarKind() == Var::DVK_Managed) {
      hipError_t err = ihipFree(elem.second->getManagedVarPtr());
      assert(err == hipSuccess);
    }
    delete elem.second;
  }
  vars_.clear();

  for (auto& elem : functions_) {
    delete elem.second;
  }
  functions_.clear();

  delete fb_info_;
}

hipError_t DynCO::getDeviceVar(DeviceVar** dvar, std::string var_name) {
  amd::ScopedLock lock(dclock_);

  CheckDeviceIdMatch();

  auto it = vars_.find(var_name);
  if (it == vars_.end()) {
    LogPrintfError("Cannot find the Var: %s ", var_name.c_str());
    return hipErrorNotFound;
  }

  hipError_t err = it->second->getDeviceVar(dvar, device_id_, module());
  return err;
}

hipError_t DynCO::getDynFunc(hipFunction_t* hfunc, std::string func_name) {
  amd::ScopedLock lock(dclock_);

  CheckDeviceIdMatch();

  if (hfunc == nullptr) {
    return hipErrorInvalidValue;
  }

  auto it = functions_.find(func_name);
  if (it == functions_.end()) {
    LogPrintfError("Cannot find the function: %s ", func_name.c_str());
    return hipErrorNotFound;
  }

  /* See if this could be solved */
  return it->second->getDynFunc(hfunc, module());
}

hipError_t DynCO::initDynManagedVars(const std::string& managedVar) {
  amd::ScopedLock lock(dclock_);
  DeviceVar* dvar;
  void* pointer = nullptr;
  hipError_t status = hipSuccess;
  // To get size of the managed variable
  status = getDeviceVar(&dvar, managedVar + ".managed");
  if (status != hipSuccess) {
    ClPrint(amd::LOG_ERROR, amd::LOG_API, "Status %d, failed to get .managed device variable:%s",
            status, managedVar.c_str());
    return status;
  }
  // Allocate managed memory for these symbols
  status = ihipMallocManaged(&pointer, dvar->size());
  guarantee(status == hipSuccess, "Status %d, failed to allocate managed memory", status);

  // update as manager variable and set managed memory pointer and size
  auto it = vars_.find(managedVar);
  it->second->setManagedVarInfo(pointer, dvar->size());

  // copy initial value to the managed variable to the managed memory allocated
  hip::Stream* stream = hip::getNullStream();
  if (stream != nullptr) {
    status = ihipMemcpy(pointer, reinterpret_cast<address>(dvar->device_ptr()), dvar->size(),
                        hipMemcpyDeviceToDevice, *stream);
    if (status != hipSuccess) {
      ClPrint(amd::LOG_ERROR, amd::LOG_API, "Status %d, failed to copy device ptr:%s", status,
              managedVar.c_str());
      return status;
    }
  } else {
    ClPrint(amd::LOG_ERROR, amd::LOG_API, "Host Queue is NULL");
    return hipErrorInvalidResourceHandle;
  }

  // Get deivce ptr to initialize with managed memory pointer
  status = getDeviceVar(&dvar, managedVar);
  if (status != hipSuccess) {
    ClPrint(amd::LOG_ERROR, amd::LOG_API, "Status %d, failed to get managed device variable:%s",
            status, managedVar.c_str());
    return status;
  }
  // copy managed memory pointer to the managed device variable
  status = ihipMemcpy(reinterpret_cast<address>(dvar->device_ptr()), &pointer, dvar->size(),
                      hipMemcpyHostToDevice, *stream);
  if (status != hipSuccess) {
    ClPrint(amd::LOG_ERROR, amd::LOG_API, "Status %d, failed to copy device ptr:%s", status,
            managedVar.c_str());
    return status;
  }
  return status;
}

hipError_t DynCO::populateDynGlobalVars() {
  amd::ScopedLock lock(dclock_);
  hipError_t err = hipSuccess;
  std::vector<std::string> var_names;
  std::string managedVarExt = ".managed";
  // For Dynamic Modules there is only one hipFatBinaryDevInfo_
  device::Program* dev_program = fb_info_->GetProgram(ihipGetDevice())
                                     ->getDeviceProgram(*hip::getCurrentDevice()->devices()[0]);

  if (!dev_program->getGlobalVarFromCodeObj(&var_names)) {
    LogPrintfError("Could not get Global vars from Code Obj for Module: 0x%x", module());
    return hipErrorSharedObjectSymbolNotFound;
  }

  for (auto& elem : var_names) {
    vars_.insert(
        std::make_pair(elem, new Var(elem, Var::DeviceVarKind::DVK_Variable, 0, 0, 0, nullptr)));
  }

  for (auto& elem : var_names) {
    if (elem.find(managedVarExt) != std::string::npos) {
      std::string managedVar = elem;
      managedVar.erase(managedVar.length() - managedVarExt.length(), managedVarExt.length());
      err = initDynManagedVars(managedVar);
    }
  }
  return err;
}

hipError_t DynCO::populateDynGlobalFuncs() {
  amd::ScopedLock lock(dclock_);

  std::vector<std::string> func_names;
  device::Program* dev_program = fb_info_->GetProgram(ihipGetDevice())
                                     ->getDeviceProgram(*hip::getCurrentDevice()->devices()[0]);

  // Get all the global func names from COMGR
  if (!dev_program->getGlobalFuncFromCodeObj(&func_names)) {
    LogPrintfError("Could not get Global Funcs from Code Obj for Module: 0x%x", module());
    return hipErrorSharedObjectSymbolNotFound;
  }

  for (auto& elem : func_names) {
    functions_.insert(std::make_pair(elem, new Function(elem)));
  }

  return hipSuccess;
}

// Static Code Object
StatCO::StatCO() {}

StatCO::~StatCO() {
  amd::ScopedLock lock(sclock_);

  for (auto& elem : functions_) {
    delete elem.second;
  }
  functions_.clear();

  for (auto& elem : vars_) {
    delete elem.second;
  }
  vars_.clear();
}

hipError_t StatCO::digestFatBinary(const void* data, FatBinaryInfo*& programs) {
  amd::ScopedLock lock(sclock_);

  if (programs != nullptr) {
    return hipSuccess;
  }

  // Create a new fat binary object and extract the fat binary for all devices.
  programs = new FatBinaryInfo(nullptr, data);
  IHIP_RETURN_ONFAIL(programs->ExtractFatBinary(g_devices));

  return hipSuccess;
}

FatBinaryInfo** StatCO::addFatBinary(const void* data, bool initialized) {
  amd::ScopedLock lock(sclock_);

  if (initialized) {
    hipError_t err = digestFatBinary(data, modules_[data]);
    assert(err == hipSuccess);
  }
  return &modules_[data];
}

hipError_t StatCO::removeFatBinary(FatBinaryInfo** module) {
  amd::ScopedLock lock(sclock_);

  auto vit = vars_.begin();
  while (vit != vars_.end()) {
    if (vit->second->moduleInfo() == module) {
      delete vit->second;
      vit = vars_.erase(vit);
    } else {
      ++vit;
    }
  }

  auto it = managedVars_.begin();
  while (it != managedVars_.end()) {
    if ((*it)->moduleInfo() == module) {
      for (auto dev : g_devices) {
        DeviceVar* dvar = nullptr;
        IHIP_RETURN_ONFAIL((*it)->getStatDeviceVar(&dvar, dev->deviceId()));
        // free also deletes the device ptr
        hipError_t err = ihipFree(dvar->device_ptr());
        assert(err == hipSuccess);
      }
      it = managedVars_.erase(it);
    } else {
      ++it;
    }
  }

  auto fit = functions_.begin();
  while (fit != functions_.end()) {
    if (fit->second->moduleInfo() == module) {
      delete fit->second;
      fit = functions_.erase(fit);
    } else {
      ++fit;
    }
  }

  auto mit = modules_.begin();
  while (mit != modules_.end()) {
    if (&mit->second == module) {
      delete mit->second;
      mit = modules_.erase(mit);
    } else {
      ++mit;
    }
  }

  return hipSuccess;
}

hipError_t StatCO::registerStatFunction(const void* hostFunction, Function* func) {
  amd::ScopedLock lock(sclock_);

  if (functions_.find(hostFunction) != functions_.end()) {
    DevLogPrintfError("hostFunctionPtr: 0x%x already exists", hostFunction);
  }
  functions_.insert(std::make_pair(hostFunction, func));

  return hipSuccess;
}

const char* StatCO::getStatFuncName(const void* hostFunction) {
  amd::ScopedLock lock(sclock_);

  const auto it = functions_.find(hostFunction);
  if (it == functions_.end()) {
    return nullptr;
  }
  return it->second->name().c_str();
}

hipError_t StatCO::getStatFunc(hipFunction_t* hfunc, const void* hostFunction, int deviceId) {
  amd::ScopedLock lock(sclock_);

  const auto it = functions_.find(hostFunction);
  if (it == functions_.end()) {
    return hipErrorInvalidSymbol;
  }

  return it->second->getStatFunc(hfunc, deviceId);
}

hipError_t StatCO::getStatFuncAttr(hipFuncAttributes* func_attr, const void* hostFunction,
                                   int deviceId) {
  amd::ScopedLock lock(sclock_);

  const auto it = functions_.find(hostFunction);
  if (it == functions_.end()) {
    return hipErrorInvalidSymbol;
  }

  return it->second->getStatFuncAttr(func_attr, deviceId);
}

hipError_t StatCO::registerStatGlobalVar(const void* hostVar, Var* var) {
  amd::ScopedLock lock(sclock_);

  if (vars_.find(hostVar) != vars_.end()) {
    return hipErrorInvalidSymbol;
  }

  vars_.insert(std::make_pair(hostVar, var));
  return hipSuccess;
}

hipError_t StatCO::getStatGlobalVar(const void* hostVar, int deviceId, hipDeviceptr_t* dev_ptr,
                                    size_t* size_ptr) {
  amd::ScopedLock lock(sclock_);

  const auto it = vars_.find(hostVar);
  if (it == vars_.end()) {
    return hipErrorInvalidSymbol;
  }

  DeviceVar* dvar = nullptr;
  IHIP_RETURN_ONFAIL(it->second->getStatDeviceVar(&dvar, deviceId));

  *dev_ptr = dvar->device_ptr();
  *size_ptr = dvar->size();
  return hipSuccess;
}

hipError_t StatCO::registerStatManagedVar(Var* var) {
  managedVars_.emplace_back(var);
  return hipSuccess;
}

hipError_t StatCO::initStatManagedVarDevicePtr(int deviceId) {
  amd::ScopedLock lock(sclock_);
  hipError_t err = hipSuccess;
  if (managedVarsDevicePtrInitalized_.find(deviceId) == managedVarsDevicePtrInitalized_.end() ||
      !managedVarsDevicePtrInitalized_[deviceId]) {
    for (auto var : managedVars_) {
      DeviceVar* dvar = nullptr;
      IHIP_RETURN_ONFAIL(var->getStatDeviceVar(&dvar, deviceId));

      hip::Stream* stream = g_devices.at(deviceId)->NullStream();
      if (stream != nullptr) {
        err = ihipMemcpy(reinterpret_cast<address>(dvar->device_ptr()), var->getManagedVarPtr(),
                         dvar->size(), hipMemcpyHostToDevice, *stream);
      } else {
        ClPrint(amd::LOG_ERROR, amd::LOG_API, "Host Queue is NULL");
        return hipErrorInvalidResourceHandle;
      }
    }
    managedVarsDevicePtrInitalized_[deviceId] = true;
  }
  return err;
}
}  // namespace hip
