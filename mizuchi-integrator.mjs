/**
 * Mizuchi Integrator Module for Klonoa: Empire of Dreams
 *
 * 1. Copies gitignored build artifacts into the worktree
 * 2. Replaces the INCLUDE_ASM stub with decompiled C code
 * 3. Moves the matched .s file from nonmatchings/ to matchings/
 */

import { execSync } from 'child_process';
import fs from 'fs';
import path from 'path';

/**
 * Find which module a function belongs to by scanning src/*.c for its INCLUDE_ASM.
 * Returns the module name (the first arg to INCLUDE_ASM, e.g. "asm/nonmatchings/math" → "math").
 */
function findModule(worktreePath, functionName) {
  const srcDir = path.join(worktreePath, 'src');
  const pattern = new RegExp(`INCLUDE_ASM\\("asm/nonmatchings/(\\w+)",\\s*${functionName}\\)`);

  for (const file of fs.readdirSync(srcDir)) {
    if (!file.endsWith('.c')) continue;
    const content = fs.readFileSync(path.join(srcDir, file), 'utf-8');
    const match = pattern.exec(content);
    if (match) return match[1];
  }
  return null;
}

/**
 * Copy gitignored build artifacts from the project root into the worktree.
 * Git worktrees don't include gitignored files or submodules.
 */
function setupWorktree(projectRoot, worktreePath, helpers) {
  helpers.log('Setting up worktree...');

  // Copy gitignored files
  fs.copyFileSync(path.join(projectRoot, 'baserom.gba'), path.join(worktreePath, 'baserom.gba'));
  fs.copyFileSync(path.join(projectRoot, 'ctx.c'), path.join(worktreePath, 'ctx.c'));

  // Merge asm/ and data/ into worktree (asm/ has checked-in files like crt0.s)
  execSync(`rsync -a "${projectRoot}/asm/" "${worktreePath}/asm/"`, { stdio: 'pipe' });
  execSync(`rsync -a "${projectRoot}/data/" "${worktreePath}/data/"`, { stdio: 'pipe' });

  // Symlink submodules (tools/agbcc, tools/luvdis) — they are empty placeholder
  // dirs in worktrees. Replace with symlinks to the main tree's populated submodules.
  for (const sub of ['tools/agbcc', 'tools/luvdis']) {
    const src = path.join(projectRoot, sub);
    const dest = path.join(worktreePath, sub);
    if (fs.existsSync(src)) {
      fs.rmSync(dest, { recursive: true, force: true });
      fs.symlinkSync(src, dest);
    }
  }
}

export async function integrate({ functionName, generatedCode, worktreePath, projectRoot, helpers }) {
  // Figure out the module before we modify anything
  const module = findModule(worktreePath, functionName);

  // Populate the worktree with gitignored build artifacts
  setupWorktree(projectRoot, worktreePath, helpers);

  // Strip extern/forward declarations that already exist in the target file
  // (e.g. multiple decompiled functions may reference the same external symbol)
  const srcFile = helpers.findSourceFile(functionName);
  const cleanedCode = helpers.stripDuplicateDeclarations(srcFile, generatedCode);

  // Replace the INCLUDE_ASM stub with the decompiled C code
  helpers.replaceIncludeAsm(srcFile, functionName, cleanedCode);

  // Move the .s file from nonmatchings/ to matchings/
  // (mirrors what generate_asm.py's _move_matched_functions does)
  if (module) {
    const nmFile = path.join(worktreePath, 'asm', 'nonmatchings', module, `${functionName}.s`);
    const matchDir = path.join(worktreePath, 'asm', 'matchings', module);

    if (fs.existsSync(nmFile)) {
      fs.mkdirSync(matchDir, { recursive: true });
      fs.renameSync(nmFile, path.join(matchDir, `${functionName}.s`));
      helpers.log(`Moved ${functionName}.s to asm/matchings/${module}/`);
    }
  }

  return {
    filesModified: [srcFile],
    summary: `Replaced INCLUDE_ASM stub for ${functionName}`,
  };
}
