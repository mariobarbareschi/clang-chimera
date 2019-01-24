//===- MutationTemplate.cpp -------------------------------------*- C++ -*-===//
//
//  Copyright (C) 2015, 2016  Federico Iannucci (fed.iannucci@gmail.com)
// 
//  This file is part of Clang-Chimera.
//
//  Clang-Chimera is free software: you can redistribute it and/or modify
//  it under the terms of the GNU Affero General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  Clang-Chimera is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Affero General Public License for more details.
//
//  You should have received a copy of the GNU Affero General Public License
//  along with Clang-Chimera. If not, see <http://www.gnu.org/licenses/>.
//
//===----------------------------------------------------------------------===//
/// \file MutationTemplate.cpp
/// \author Federico Iannucci
/// \date 14 ott 2015 
/// \brief This file contains the implementation file for the class 
///        Mutation Template
//===----------------------------------------------------------------------===//

#include "Core/MutationTemplate.h"
#include "Tooling/FrontendActions.h"
#include "Tooling/CompilationDatabaseUtils.h"

#include "clang/Rewrite/Core/Rewriter.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"

#include <algorithm>

using namespace clang;
using namespace clang::tooling;
using namespace clang::ast_matchers;
using namespace chimera;
using namespace chimera::mutator;
using namespace chimera::m_operator;
using namespace chimera::log;

#define DEBUG_TYPE "mutation_template"

static mutant::IdType mutantCounterInitial = 1;


// FIXME: When a function name is not found -> LLVM IO ERROR.

///////////////////////////////////////////////////////////////////////////////
/// @brief    This class manages the creation and deletion of rewriter objects
/// @details  It works with a reservation mechanism:
///            - a slot can be reserved, when the slot is required a rewriter is
///            (eventually created and)
///              returned,
///            - if the slot doesn't exist a local over-writtable slot is used.
///            The max number of local
///              slots is given by \tparam localSlots, the max pallelisms in
///              using the Rewriters managed by this
///              class.
/// @tparam   ContentType It must have a ctor without arguments
/// FIXME Generalize it
template <typename IdType, typename ContentType, int localSlots = 1>
class SlotManager {
  using RewriterPtr =
      ::std::unique_ptr<Rewriter>; ///< Rewriter pointer abstraction
  using SlotType = ::std::unique_ptr<ContentType>;

public:
  SlotManager() {}

  /// @brief Try to reserve a slot, eg if you knew you're going to use it
  /// @param toReserve Which slot should be reserved
  /// @return bool If the reservation succeeds
  bool reserve(IdType toReserve) {
    auto retval = this->slots.insert(std::pair<IdType, SlotType>(
        toReserve, ::std::unique_ptr<ContentType>(nullptr)));
    DEBUG(::llvm::dbgs() << "Reserving id:" << toReserve
                         << ".Operation: " << retval.second << "\n");
    return retval.second;
  }

  /// @brief Set a slot, differs from reserve because it also fills the slot
  /// @param toReserve Which slot should be reserved
  /// @param content The content of the slot
  /// @return bool If the reservation succeeds
  bool setSlot(IdType toReserve, const ContentType &content) {
    auto retval = this->slots.insert(std::pair<IdType, SlotType>(
        toReserve, ::std::unique_ptr<ContentType>(new ContentType(content))));
    return retval.second;
  }

  /// @brief Try to reserve a slot using the local slot if it is valid.
  /// @return bool If the reservation succeeds
  bool reserveLocalSlot() {
    // Check localSlot
    if (this->localSlot.second) {
      auto retval = this->slots.insert(::std::move(localSlot));
      return retval.second;
    }
    return false;
  }

  /// @brief Try to release a previously reserved slot
  /// @param toRelease
  /// @return bool If the release succeeds
  bool release(IdType toRelease) { return this->slots.erase(toRelease) == 1; }

  /// @brief Retrieve a reserved slot if exists
  /// @param slotId
  /// @param content
  /// @return bool If it exist
  bool getReservedSlot(IdType slotId, ContentType &content) {
    try {
      content = *(this->slots.at(slotId));
      return true;
    } catch (const std::out_of_range &oor) { // Not present
      return false;
    }
  }

  /// @brief It creates the content of a slot, of a local one if wasn't reserved
  /// or of the reserved one.
  ///        If the content of the reserved slot already exist, it returns it.
  /// @param mngr
  /// @param lang
  /// @param wasReserved If the slot was reserved
  /// @return Rewriter A rewriter
  template <typename... Args>
  ContentType &getSlot(IdType slot, bool &wasReserved, Args &&... args) {
    wasReserved = true;
    try {
      // Try to access the object, seeing if manages an object
      if (!this->slots.at(slot)) {
        // Create one
        this->slots.at(slot)
            .reset(new ContentType(::std::forward<Args>(args)...));
      }

      return *(this->slots.at(slot));
    } catch (const std::out_of_range &oor) {
      wasReserved = false;
      // Renew the localSlot
      localSlot.first = slot;
      localSlot.second.reset(new ContentType(::std::forward<Args>(args)...));
      return *localSlot.second;
    }
  }

private:
  // Each rewriter is associated with a key, in this case the mutantId
  ::std::map<IdType, SlotType> slots;
  ::std::pair<IdType, SlotType> localSlot; // Local slot
};

static SlotManager<mutant::IdType, Rewriter> rwManager;

///////////////////////////////////////////////////////////////////////////////
/// @brief MatchCallback child : The callback called for the mutator's matchers
class MutatorMatcherCallback : public MatchFinder::MatchCallback {
public:
  /**
   * @brief Constructor, save a pointer to the MutationTemplate from which it
   * has been created
   */
  MutatorMatcherCallback(MutationTemplate &mutTempl, MutatorPtr mutator,
                         mutant::IdType staticId = 0,
                         std::string tempDirName = "temp")
      : MatchCallback(), mutationTemplate(mutTempl), mutator(mutator),
        sourceManager(nullptr), context(nullptr), localMutantId(staticId),
        tempDirName(tempDirName) {}

  /// @brief Set the local pointer to the source manager
  /// @param manager A pointer to the source manager
  void setSourceManager(SourceManager *manager) {
    if (this->sourceManager == nullptr) {
      this->sourceManager = manager;
    }
  }

  /// @brief Set the local pointer to the ASTContext
  /// @param manager A pointer to the source manager
  void setASTContext(ASTContext *c) {
    if (this->context == nullptr) {
      this->context = c;
    }
  }

  /// @brief  This method initializes all the information required to generate a
  /// mutant. In particular the
  ///         retrieving of the mutant id and the associated rewriter. The
  ///         behaviour is specialized on the
  ///         mutator type FOM or HOM.
  /// @details In case of FOM mutators, the default type, for each call a new
  /// rewriter is generated.
  ///          In case of HOM mutators only one rewriter will be created for
  ///          evey match/mutation,
  ///          this call become idempotent.
  ///          The localMutantId could or couldn't be initialized, in the latter
  ///          case its default value is 0.
  ///          When it's default it means either is a FOM or a mutations has not
  ///          succeeded yet.
  ///          Only when it isn't initialized if this method is called it is set
  ///          the the mutantCounter.
  ///          If the mutant fails some step, the value is set back to default,
  ///          until at least one mutations succeeds
  Rewriter &initializeMutant(mutant::IdType &id) {
    id = this->localMutantId;
    if (id == 0) {
      // As for the FOM mutator
      id = this->mutationTemplate.mutantCounter;
    }
    bool wasReserved;
    return rwManager.getSlot(id, wasReserved, *(this->sourceManager),
                             this->context->getLangOpts());
  }

  /// @brief Called when a mutant has been created, it finalizes the used
  /// information.
  ///        It performs the following:
  ///         - Set the local mutant id incrementing the mutant counter, or only
  ///         the latter
  ///         - Reserve the used rewriter
  void finalizeMutant() {
    if (this->mutator->isHom()) {
      // HOM
      if (this->localMutantId == 0) {
        // If the localMutantId was 0, it has to be set and ...
        this->localMutantId = this->mutationTemplate.mutantCounter++;
        // ... the rewriter reserved
        rwManager.reserveLocalSlot();
      }
    } else
      // FOM, increment and do nothing
      this->mutationTemplate.mutantCounter++;
  }

  /// @brief Apply mutations and report and/or save them according to the state
  /// of the mutation template.
  /// @details This function generates automatically the report. If report and
  /// mutants haven't to be saved,
  ///          this functions shouldn't be called.
  ///          It works with both FOM and HOM mutators.
  /// @param Result MatchResult object
  void applyMutations(const MatchFinder::MatchResult &Result) {
    // Local variables
    mutant::IdType mutantId; // Mutant Id
    ::clang::ast_type_traits::DynTypedNode
        matchedNode; // It will contain the matched node
    // Matched node validty
    bool nodeIsValid = this->mutator->getMatchedNode(Result, matchedNode);

    // Loop on mutator types
    for (MutatorType i = 0; i < this->mutator->getTypes(); ++i) {
      // Per mutation type actions:
      // * Set local mutantId and retrieve a rewriter
      Rewriter &localRw = this->initializeMutant(mutantId);

      // Verbose messages
      if (nodeIsValid) {
        ChimeraLogger::verbose(
            "[" + std::to_string(mutantId) + "] Applying mutation in " +
            matchedNode.getSourceRange().getBegin().printToString(
                *(this->sourceManager)));
      } else {
        ChimeraLogger::verbose("[" + std::to_string(mutantId) +
                               "] Applying mutation in <invalid>. Report for "
                               "this mutant will not be generated");
      }

      // Apply the mutation calling the mutate method
      this->mutator->mutate(Result, i, localRw);

      // Check if actually a rewriteBuffer has been created, id est if the
      // buffer has been modified.
      if (localRw.getRewriteBufferFor(localRw.getSourceMgr().getMainFileID()) !=
          nullptr) {
        // The source file has been somehow modified, continue
        // Check if the mutant is valid
        ChimeraLogger::verboseAndIncr("[" + std::to_string(mutantId) +
                                      "][ RUN  ] Checking mutant");

        if (this->checkMutant(localRw)) {
          ChimeraLogger::verbosePreDecr("[" + std::to_string(mutantId) +
                                        "][ PASS ] Checking mutant");

          // The mutant is valid, continue
          // Save the report if the matched node is valid
          if (nodeIsValid) {
            this->createReportEntry(
                mutantId, Result.Nodes.getNodeAs<FunctionDecl>("functionDecl")
                              ->getNameAsString(),
                matchedNode.getSourceRange().getBegin(),
                this->mutator->getIdentifier(), i);
          }

          // Save the mutant to file if this feature is enabled
          if (this->mutationTemplate.isGenerateMutants()) {
            this->saveMutant(mutantId, localRw);
          } else {
            ChimeraLogger::verbose("[" + std::to_string(mutantId) +
                                   "] Saving disabled");
          }
          // Increment mutantCounter if the mutator is not an HOM
          this->finalizeMutant();
        } else {
          // The mutant is invalid
          ChimeraLogger::verbosePreDecr("[" + std::to_string(mutantId) +
                                        "][ FAIL ] Checking mutant");
#ifdef _CHIMERA_DEBUG_
          // DEBUG
          rw.getEditBuffer(rw.getSourceMgr().getMainFileID())
              .write(llvm::outs());
#endif
        }
      } else {
        ChimeraLogger::verbose("[" + std::to_string(mutantId) +
                               "] Application didn't produce changes");
      }
      //      this->deleteLocalRewriter();  // Delete the rewriter
    }
  }

  /// @brief Save a mutant given an unique id and the Rewriter that contains the
  /// sourceBuffer
  /// @param id Mutant unique id
  /// @param rewriter Rewriter object with RewriteBuffer that contains the
  /// source modification
  /// @return If the Mutant is correctly saved
  bool saveMutant(mutant::IdType id, Rewriter &rewriter) {
    std::string filename(this->mutationTemplate.getTargetFilename().data());
    std::string mutantPath = this->mutationTemplate.getTargetOutputDirectory() +
                             std::to_string(id) + chimera::fs::pathSep;
    std::string filePath = mutantPath + filename;
    ChimeraLogger::verbose("[" + std::to_string(id) + "] Saving mutant in " +
                           clang::tooling::getAbsolutePath(filePath));

    // Create folder for this mutant
    chimera::fs::createDirectories(mutantPath);

    // Save mutant on file
    // Create the file stream
    ::std::error_code fileError;
    llvm::raw_fd_ostream file(filePath.c_str(), fileError,
                              llvm::sys::fs::F_Text);
    if (!file.has_error()) {
      rewriter.getEditBuffer(rewriter.getSourceMgr().getMainFileID())
          .write(file);
    } else {
      ChimeraLogger::error("An error occurred during the file opening: " +
                           fileError.message());
      return false;
    }
    file.close(); // Close the file stream
    return true;
  }

  /// @brief Check syntactically a mutant identified by id
  /// @param id Mutant unique id
  /// @return If the mutant passes the check
  bool checkMutant(Rewriter &rw) {
    // Create a temp directory and a temp file
    std::string tempDir = this->mutationTemplate.getTargetOutputDirectory() +
                          this->tempDirName + chimera::fs::pathSep;
    std::string tempFilePath =
        tempDir + this->mutationTemplate.getTargetFilename().data();

    // Create the directory
    chimera::fs::createDirectories(tempDir);

    // Create temp file
    ::std::error_code fileError;
    ::llvm::raw_fd_ostream tempFile(tempFilePath, fileError,
                                    llvm::sys::fs::F_Text);
    if (!tempFile.has_error()) {
      // Write the temp file
      rw.getEditBuffer(rw.getSourceMgr().getMainFileID()).write(tempFile);
      tempFile.close(); // Close the file stream

      ChimeraLogger::verbose("Building CompilationDatabase");
      // Get compileCommands for this target
      CompileCommand command = this->mutationTemplate.getCompileCommand();

      // Modify the compile command
      ::chimera::cd_utils::changeCompileCommandTarget(
          command, this->mutationTemplate.getTargetPath(), tempFilePath, true);

      // Adding compile commands from the mutator
      const ::std::vector<::std::string> &mutatorCompileCommands =
          this->mutator->getAdditionalCompileCommands();
      command.CommandLine.insert(command.CommandLine.end(),
                                 mutatorCompileCommands.begin(),
                                 mutatorCompileCommands.end());

#ifdef _CHIMERA_DEBUG_
      chimera::cd_utils::dump(std::cout, commands); // Debug
#endif
      ChimeraLogger::verbose("Running syntax check");

      // TODO A frontend action can be called directly on code in string format
      // clang::tooling::runToolOnCode uses the -fsyntax-only code that check
      // only the syntax
      // BUT show always errors for inclusion not found, because compile
      // commands aren't passed in input.
      return chimera::checkSyntaxAction(
                 command, this->mutationTemplate.getTargetPath()) == 0;
    } else {
      ChimeraLogger::error("An error occurred during the file opening: " +
                           fileError.message());
      return false;
    }
  }

  /// @brief Delete a mutant that fails the check
  /// @param id Mutant unique id
  void deleteMutant(mutant::IdType id) {
    // Create mutant directory
    std::string mutantDir = this->mutationTemplate.getTargetOutputDirectory() +
                            std::to_string(id) + chimera::fs::pathSep;
    // Delete the mutant file
    llvm::sys::fs::remove(mutantDir +
                          this->mutationTemplate.getTargetFilename().data());
    // Delete the mutant folder
    llvm::sys::fs::remove(mutantDir);
  }

  /// @brief create an entry for the mutants report file
  /// @details Report's entry format :
  ///          - Mutant Id
  ///          - Location
  ///          - Mutator Identifier
  void createReportEntry(mutant::IdType id, const std::string &functionName,
                         const SourceLocation &l,
                         const std::string &mutatorIdentifier,
                         mutator::MutatorType type) {
    ChimeraLogger::verbose("[" + std::to_string(id) +
                           "] Mutant report: Location: " +
                           l.printToString(*(this->sourceManager)));
    // Create a fullSource -> a SourceLocation with an associatd SourceManager
    FullSourceLoc fullLoc(l, *(this->sourceManager));
    this->mutationTemplate.getReportStream()
        << id << "," << functionName << "," << fullLoc.getSpellingLineNumber()
        << "," << fullLoc.getSpellingColumnNumber() << "," << mutatorIdentifier
        << "," << type << std::endl;
  }

  ///////////////////////////////////////////////////////////////////////////////
  /// Virtual functions implementation for MatchCallback class
  /**
   * @brief Run implementation. This function is called when a coarse match is
   * found.
   *        In case of also fine grain match, it performs the actions required
   * to generate the mutants.
   */
  virtual void run(const MatchFinder::MatchResult &Result) {
    ChimeraLogger::verboseAndIncr("Coarse grain matching from " +
                                  this->mutator->getIdentifier());
    // Set the local sourceManager
    this->setSourceManager(Result.SourceManager);
    this->setASTContext(Result.Context);
    // Apply fine grained matching rules
    if (this->mutator->match(Result)) {
      // It is very likely that mutants have to be created -> general mutant
      ChimeraLogger::verboseAndIncr("Fine grain matching [ PASS ]");

      // With the introduction of the HOM mutators, this phase has to be
      // specialized
      this->applyMutations(Result);

      ChimeraLogger::decrActualVLevel();
    } else {
      ChimeraLogger::verbose("Fine grain matching [ FAIL ]");
    }
    ChimeraLogger::decrActualVLevel();
  }
  /**
   * @brief Per TranslationUnit task
   */
  virtual void onEndOfTranslationUnit() {
    //    ChimeraLogger::verbose(" [ RUN  ] Cleaning up");
    // Call callbacks: if the mutator is HOM, and so the localMutantId is != 0.
    // Finally the mutant directory exists only if the mutants have been
    // generated.
    if (this->mutator->isHom() && this->localMutantId != 0 &&
        this->mutationTemplate.isGenerateMutants()) {
      // At this point the mutant has been created
      this->mutator->onCreatedMutant(
          this->mutationTemplate.getTargetOutputDirectory() +
          ::std::to_string(this->localMutantId) + ::chimera::fs::pathSep);
    }

    // // Delete temp folder, deleting all files inside
    // ::std::string tempDir = this->mutationTemplate.getTargetOutputDirectory() +
    //                         this->tempDirName + chimera::fs::pathSep;
    // // Delete temp file for syntax checking
    // ::llvm::sys::fs::remove(tempDir +
    //                         this->mutationTemplate.getTargetFilename());

    // // Delete the temp directory
    // ::llvm::sys::fs::remove(tempDir);

    //    ChimeraLogger::verbose(" [ DONE ] Cleaning up");
  }

private:
  MutationTemplate &mutationTemplate; ///< Reference to the mutation template
  MutatorPtr mutator;                 ///< Mutator related to this Matcher
  SourceManager *sourceManager;       ///< Pointer to the source manager
  const ASTContext *context;
  /// @brief In case of HOM mutator, this attribute could be externally provided
  ///        and it represents a "reserved" id that identifies a mutant. The id
  ///        influences the retrieve
  ///        of the rewriter.
  mutant::IdType localMutantId;
  const ::std::string tempDirName; ///< Temporary directory
};

///////////////////////////////////////////////////////////////////////////////
// Class MutationTemplate Implementation

static SlotManager<m_operator::IdType, mutant::IdType> idManager;

// Private methods
void chimera::MutationTemplate::initMutantIds_() {
  // Reset slot manager
  idManager = SlotManager<m_operator::IdType, mutant::IdType>();
  rwManager = SlotManager<mutant::IdType, Rewriter>();
  // Reset mutant counter
  this->mutantCounter = mutantCounterInitial;
  // Loop on operators to find HOM and reserve their ids.
  // The hypothesis is that they are going to be used, ie at least one mutation.
  mutant::IdType reservedId;
  for (auto op : this->operators) {
    if (op.second->isHom()) {
      // Set a slot that binds operator and an identifier, that will be used for
      // all its HOM mutators
      reservedId = this->mutantCounter;
      if (!idManager.setSlot(op.second->getIdentifier(), reservedId) ||
          !rwManager.reserve(reservedId)) {
        ChimeraLogger::fatal("Couldn't reserve a mutantId for an operator. "
                             "Maybe a mutantId duplicate or memory issues.");
      }
      this->mutantCounter++;
    }
  }
}

/// @brief Add matchers to finder from mutators vector using functionName as
/// function's name filter.
/// @param finder The Match finder in which add the Matchers
/// @param operatorId The operator id of which take the matchers
/// @param functionName The name of the target function/method
void chimera::MutationTemplate::addMatchers_(
    MatchFinder &finder, const m_operator::IdType &operatorId,
    const std::string &functionName) {
  // Manage FOM and HOM operator, the mutators are managed inside the callback
  mutant::IdType reservedId = 0;
  if (this->operators.at(operatorId)->isHom()) {
    // Retrieve reservedId
    if (!idManager.getReservedSlot(operatorId, reservedId)) {
      ChimeraLogger::fatal("An id wasn't reserved for this operator.");
    }
  }
  const auto &mutators = this->operators[operatorId]->getMutators();
  // Check functionName
  internal::Matcher<NamedDecl> functionHasName = anything();
  if (functionName != "") {
    // Filter on function's name
    functionHasName = hasName(functionName);
  }
  // Loop on mutators
  for (unsigned j = 0; j < mutators.size(); ++j) {
    // Create the callback for this mutator
    // TODO Manage deallocation of callbackObj
    MutatorMatcherCallback *callbackObj =
        new MutatorMatcherCallback(*this, mutators[j], reservedId);
    /// The Mutation Template passes to the mutator through bind() the
    /// functionDecl reference.
    /// This DeclarationMatcher is a wrapper to reduce the mutations only to the
    /// functions with
    /// body.
    DeclarationMatcher functionDefMatcher = anything();
    std::string functionDefId =
        "functionDecl"; /// Id for the matchCallback bind.
    /* Switch on mutator matcher type */
    switch (mutators[j]->getMatcherType()) {
    case StatementMatcherType:
      functionDefMatcher =
          functionDecl(functionHasName, isDefinition(),
                       forEachDescendant(mutators[j]->getStatementMatcher()))
              .bind(functionDefId);
      break;
    case DeclarationMatcherType:
      functionDefMatcher =
          functionDecl(functionHasName, isDefinition(),
                       forEachDescendant(mutators[j]->getDeclarationMatcher()))
              .bind(functionDefId);
      break;
    default:
      llvm_unreachable("Matcher Type unsupported");
      break;
    }
    // Add the matcher to the finder
    finder.addMatcher(functionDefMatcher, callbackObj);
  }
}

/// @brief Add matchers from local operators filtered by identifier
void chimera::MutationTemplate::addMatchers_(
    MatchFinder &finder, const std::vector<m_operator::IdType> &operatorIds,
    const std::string &functionName) {
  // Check if there is CHIMERA_ALL_OPERATORS
  auto operatorId = std::find(operatorIds.begin(), operatorIds.end(),
                              "CHIMERA_ALL_OPERATORS");
  if (operatorId != operatorIds.end()) {
    // Found, Apply all operators
    ChimeraLogger::verbose("Found CHIMERA_ALL_OPERATORS");
    {
      // Load matcher from all operators
      for (auto op = this->operators.begin(); op != this->operators.end();
           ++op) {
        this->addMatchers_(finder, op->first, functionName);
      }
    }
  } else {
    // Load matcher from filtered operators
    for (operatorId = operatorIds.begin(); operatorId != operatorIds.end();
         ++operatorId) {
      /// Take the operator's mutators and add their matcher to
      /// MutationTemplate's MatchFinder.
      std::cout << "Operator : " << *operatorId << std::endl;
      // Check if the operator exists
      auto op = this->operators.find(*operatorId);
      if (op != this->operators.end()) {
        this->addMatchers_(finder, op->first, functionName);
      }
    }
  }
}

/// @brief Run the internal ClangTool on a MatchFinder
/// @details Perform all operations needed before/after the ClangTool.run call.
/// @param finder The MatchFinder to use to create the FrontendAction
/// @return 0 OK
///         1 Not OK - Some error occured
int chimera::MutationTemplate::run(clang::ast_matchers::MatchFinder &finder) {
  int retval = 1; // Default error
  if (isGenerateMutants() || isGenerateMutantsReport()) {
    ChimeraLogger::verboseAndIncr("[ RUN  ] Internal tool");
    
    // Create output folder
    ChimeraLogger::verbose(
        "Creating output folder " +
        clang::tooling::getAbsolutePath(this->getTargetOutputDirectory()));
    if (!chimera::fs::createDirectories(this->getTargetOutputDirectory())) {
      ChimeraLogger::fatal("Couldn't create output folder");
      return 1;
    }
    
    // Open report stream
    if (this->openReportStream("report.csv")) {
      // retval = this->tool.run(newFrontendActionFactory(&finder).get());
      // Run the ClangTool on a Finder FrontendAction
      // FIXME: Instead of using the ClantTool it coulbe be used directly the
      // CompilerInvocation.
      
      retval = (ClangTool(::chimera::cd_utils::FlexibleCompilationDatabase(
                              this->compileCommand),
                          this->targetPath))
                   .run(newFrontendActionFactory(&finder).get());

      this->closeReportStream();

      // After-run tasks:
      // * Call onEndOfTranslationUnit on mutators
      //      ::std::for_each(
      //          this->operators.cbegin(), this->operators.cend(),
      //          [this] (const ::std::pair<m_operator::IdType, OperatorPtr>& p)
      //          {
      //            const auto& v = p.second->getMutators();
      //            ::std::for_each(v.cbegin(), v.cend(), [this] (MutatorPtr m)
      //            {
      //                  m->onEndOfTranslationUnit(this->getTargetOutputDirectory());
      //                });
      //          });

    } else {
      ChimeraLogger::fatal("Couldn't open the report file");
    }
    ChimeraLogger::decrActualVLevel();
    ChimeraLogger::verbose("[ DONE ] Internal tool");
  } else {
    ChimeraLogger::verbose("[ SKIP ] Running internal tool is useless, nor "
                           "mutants or report have to be saved. Skipping");
  }
  return retval;
}

// Public methods implementations
chimera::MutationTemplate::MutationTemplate(
    const clang::tooling::CompileCommand &compileCommand,
    std::string targetPath, std::string outputDirectory)
    : mutantCounter(mutantCounterInitial), compileCommand(compileCommand),
      // In order to avoid multiple execution and problems with locations (they
      // became invalid)
      // the tool it's build with a CompilationDatabase with only one
      // compileCommand
      // The ClangTool.run implementation it's like that will be always executed
      // the single compileCommand
      // provided, independently of target
      tool(chimera::cd_utils::FlexibleCompilationDatabase(this->compileCommand),
           targetPath),
      generateMutantsReport(false), generateMutants(false), reportStream() {
  chimera::log::ChimeraLogger::verboseAndIncr(
      "[ RUN  ] Building MutationTemplate");
  this->setOutputDirectory(outputDirectory);
  this->setTargetPath(targetPath);
// TODO Eventually create a compileCommand merging multiple ones for the same
// target
#ifdef _CHIMERA_DEBUG_
  chimera::cd_utils::dump(std::cout, this->compileCommand);
#endif
  chimera::log::ChimeraLogger::verbosePreDecr(
      "[ DONE ] Building MutationTemplate");
}

int chimera::MutationTemplate::analyze() {
  this->initMutantIds_();
  // Create a new finder
  MatchFinder finder;
  ChimeraLogger::verbose(
      0, "FunOp Configuration file not set. Loading all operators.");
  // Load matcher from operators
  for (auto it = this->operators.begin(); it != this->operators.end(); ++it) {
    { /// Take the operator's mutators and add their matcher to
      /// MutationTemplate's MatchFinder.
      this->addMatchers_(finder, it->first);
    }
  }
  return this->run(finder);
}

int chimera::MutationTemplate::analyze(const conf::FunOpConfMap &map) {
  this->initMutantIds_();
  // Create a new finder
  MatchFinder finder;
  std::string functionName = ""; // functionName
  // Check if there is CHIMERA_ALL_FUNCTIONS specifier
  std::map<std::string, std::vector<std::string>>::const_iterator row =
      map.find("CHIMERA_ALL_FUNCTIONS");
  if (row != map.end()) {
    // Found, Apply to all functions
    ChimeraLogger::verbose("Found CHIMERA_ALL_FUNCTIONS");
    { this->addMatchers_(finder, row->second, functionName); }
  } else {
    // Not Found CHIMERA_ALL_FUNCTIONS
    // Iterate on map
    for (row = map.begin(); row != map.end(); ++row) {
      functionName = row->first; // Filter by functionName
      std::cout << "Function : " << functionName << std::endl;
      this->addMatchers_(finder, row->second, functionName);
    }
  }
  return run(finder);
}

///////////////////////////////////////////////////////////////////////////////
/// Report Stream Functions
bool chimera::MutationTemplate::openReportStream(const char *reportName) {
  this->reportStream.open(this->getTargetOutputDirectory() + reportName,
                          std::ofstream::out);
  return this->reportStream.is_open();
}

std::ostream &chimera::MutationTemplate::getReportStream() {
  return this->reportStream;
}

void chimera::MutationTemplate::closeReportStream() {
  this->reportStream.close();
}
