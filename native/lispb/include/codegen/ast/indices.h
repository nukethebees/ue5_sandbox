#pragma once

#include <lispb/index.h>

namespace lispb::codegen {

class CppTypes;
class CppFiles;
struct Modules;
struct TypeDependencyTag;
struct ExternalDeclarationTag;

namespace ir {
struct Functions;
struct Parameters;
struct MemberInitializers;
struct Enumerators;
struct SwitchCases;
}

// Zero denotes an absent type; live IDs are one-based indices into their owning CppTypes store.
using CppTypeId = Index<CppTypes>;
using CppFileId = Index<CppFiles>;
using ModuleId = Index<Modules>;
using TypeDependencyId = Index<TypeDependencyTag>;
using ExternalDeclarationId = Index<ExternalDeclarationTag>;
using FunctionId = Index<ir::Functions>;
using FunctionParameterId = Index<ir::Parameters>;
using MemberInitializerId = Index<ir::MemberInitializers>;
using EnumValueId = Index<ir::Enumerators>;
using SwitchCaseId = Index<ir::SwitchCases>;

} // namespace lispb::codegen
