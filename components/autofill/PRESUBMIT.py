# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Chromium presubmit script for src/components/autofill.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details on the presubmit API built into depot_tools.
"""

PRESUBMIT_VERSION = '2.0.0'

import re

def IsComponentsAutofillFile(f, name_suffix):
  # The exact path can change. Only check the containing folder.
  return (f.LocalPath().startswith('components/autofill/') and
          f.LocalPath().endswith(name_suffix))

def AnyAffectedFileMatches(input_api, matcher):
  return any(matcher(f) for f in input_api.change.AffectedTestableFiles())

def IsComponentsAutofillFileAffected(input_api, name_suffix):
  return AnyAffectedFileMatches(
      input_api, lambda f: IsComponentsAutofillFile(f, name_suffix))

def CheckNoBaseTimeCalls(input_api, output_api):
  """Checks that no files call base::Time::Now()."""
  pattern = input_api.re.compile(r'(base::Time::Now)\(\)')
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if (f.LocalPath().startswith('components/autofill/') and
        not f.LocalPath().endswith("PRESUBMIT.py")):
      if any(pattern.search(line) for _, line in f.ChangedContents()):
          files.append(f)

  if len(files):
    return [ output_api.PresubmitPromptWarning(
        'Consider to not call base::Time::Now() directly but use ' +
        'AutofillClock::Now(). This clock can be manipulated through ' +
        'TestAutofillClock for testing purposes, and using AutofillClock and ' +
        'throughout Autofill code makes sure Autofill tests refers to the '+
        'same (potentially manipulated) clock.',
        files) ]
  return []

def CheckNoFieldTypeCasts(input_api, output_api):
  """Checks that no files cast (e.g., raw integers to) FieldTypes."""
  pattern = input_api.re.compile(
      r'_cast<\s*FieldType\b',
      input_api.re.MULTILINE)
  files = []
  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if (f.LocalPath().startswith('components/autofill/') and
        not f.LocalPath().endswith("PRESUBMIT.py")):
      contents = input_api.ReadFile(f)
      if pattern.search(contents):
        files.append(f)

  if len(files):
    return [ output_api.PresubmitPromptWarning(
        'Do not cast raw integers to FieldType to prevent values that ' +
        'have no corresponding enum constant or are deprecated. Use '+
        'ToSafeFieldType() instead.',
        files) ]
  return []

def CheckFeatureNames(input_api, output_api):
  """Checks that no features are enabled."""

  pattern = input_api.re.compile(
          r'\bBASE_FEATURE\s*\(\s*k(\w*)\s*,\s*"(\w*)"',
          input_api.re.MULTILINE)
  warnings = []

  for f in input_api.AffectedSourceFiles(input_api.FilterSourceFile):
    if IsComponentsAutofillFile(f, 'features.cc'):
      contents = input_api.ReadFile(f)
      mismatches = [(constant, feature)
              for (constant, feature) in pattern.findall(contents)
              if constant != feature]
      if mismatches:
        mismatch_strings = ['\t{} -- {}'.format(*m) for m in mismatches]
        mismatch_string = format('\n').join(mismatch_strings)
        warnings += [ output_api.PresubmitPromptWarning(
            'Feature names should be identical to variable names:\n{}'
                .format(mismatch_string),
            [f]) ]

  return warnings

def CheckWebViewExposedExperiments(input_api, output_api):
  """Checks that changes to autofill features are exposed to webview."""

  _PRODUCTION_SUPPORT_FILE = ('android_webview/java/src/org/chromium/' +
      'android_webview/common/ProductionSupportedFlagList.java')

  warnings = []
  if (IsComponentsAutofillFileAffected(input_api, 'features.cc') and
      not AnyAffectedFileMatches(
          input_api, lambda f: f.LocalPath() == _PRODUCTION_SUPPORT_FILE)):
    warnings += [
        output_api.PresubmitPromptWarning(
            (
                'You may need to modify {} instructions if your feature affects'
                ' WebView.'
            ).format(_PRODUCTION_SUPPORT_FILE)
        )
    ]

  return warnings

def CheckModificationOfLegacyRegexPatterns(input_api, output_api):
  """Reminds to update internal regex patterns when legacy ones are modified."""

  if IsComponentsAutofillFileAffected(input_api, "legacy_regex_patterns.json"):
    return [
        output_api.PresubmitPromptWarning(
            "You may need to modify the parsing patterns in src-internal. " +
            "See go/autofill-internal-parsing-patterns for more details. " +
            "Ideally, the legacy patterns should not be modified.")
    ]

  return []

def CheckModificationOfFormAutofillUtil(input_api, output_api):
  """Reminds to keep form_autofill_util.cc and the iOS counterpart in sync."""

  if (IsComponentsAutofillFileAffected(input_api, "fill.ts") !=
      IsComponentsAutofillFileAffected(input_api, "form_autofill_util.cc")):
    return [
        output_api.PresubmitNotifyResult(
            'Form extraction/label inference has a separate iOS ' +
            'implementation in components/autofill/ios/form_util/resources/' +
            'fill.ts. Try to keep it in sync with form_autofill_util.cc.')
    ]

  return []

# Checks that UniqueRendererForm(Control)Id() is not used and suggests to use
# form_util::Get(Form|Field)RendererId() instead.
def CheckNoUsageOfUniqueRendererId(
        input_api, output_api):
  autofill_files_pattern = re.compile(
      r'(autofill|password_manager).*\.(mm|cc|h)')
  special_file = re.compile(r'form_autofill_util.cc')
  concerned_files = [(f, input_api.ReadFile(f))
                     for f in input_api.AffectedFiles(include_deletes=False)
                     if autofill_files_pattern.search(f.LocalPath())]

  warning_files = []
  renderer_id_call = re.compile(
      r'\.UniqueRendererForm(Control)?Id', re.MULTILINE)
  for autofill_file, file_content in concerned_files:
    allowed_matches = 2 if special_file.search(autofill_file.LocalPath()) else 0
    matches = re.finditer(renderer_id_call, file_content)
    if (len(list(matches)) > allowed_matches):
      warning_files.append(autofill_file)

  return [output_api.PresubmitError(
      'Do not use (Form|Field)RendererId(*.UniqueRendererForm(Control)?Id()). '
      'Consider using form_util::Get(Form|Field)RendererId(*) instead.',
      warning_files)] if len(warning_files) else []
