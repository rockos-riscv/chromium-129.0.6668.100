# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Presubmit script for sync component.

See http://dev.chromium.org/developers/how-tos/depottools/presubmit-scripts
for more details about the presubmit API built into depot_tools.
"""

import os
import re

# Some definitions don't follow all the conventions we want to enforce.
# It's either difficult or impossible to fix this, so we ignore the problem(s).
EXCEPTION_DATA_TYPES = [
  # Grandfathered types:
  'UNSPECIFIED',  # Doesn't have a root tag or notification type.
  'AUTOFILL_WALLET_DATA',  # Root tag and data type string lack DATA suffix.
  'APP_SETTINGS',  # Data type string has inconsistent capitalization.
  'EXTENSION_SETTINGS',  # Data type string has inconsistent capitalization.
  'PROXY_TABS',  # Doesn't have a root tag or notification type.
  'NIGORI',  # Data type string is 'encryption keys'.
  'SUPERVISED_USER_SETTINGS',  # Root tag and data type string replace
                               # 'Supervised' with 'Managed'

  # Deprecated types:
  'DEPRECATED_SUPERVISED_USER_ALLOWLISTS']

# Root tags are used as prefixes when creating storage keys, so certain strings
# are blocklisted in order to prevent prefix collision.
BLOCKLISTED_ROOT_TAGS = [
  '_mts_schema_descriptor'
]

# Number of distinct fields in a map entry; used to create
# sets that check for uniqueness.
MAP_ENTRY_FIELD_COUNT = 6

# String that precedes the DataType when referencing the
# proto field number enum e.g.
# sync_pb::EntitySpecifics::kManagedUserFieldNumber.
# Used to map from enum references to the DataType.
FIELD_NUMBER_PREFIX = 'sync_pb::EntitySpecifics::k'

# Start and end regexes for finding the EntitySpecifics definition in
# entity_specifics.proto.
PROTO_DEFINITION_START_PATTERN = '^  oneof specifics_variant \{'
PROTO_DEFINITION_END_PATTERN = '^\}'

# Start and end regexes for finding the DataTypeInfoMap definition
# in data_type.cc.
DATA_TYPE_START_PATTERN = '^const DataTypeInfo kDataTypeInfoMap'
DATA_TYPE_END_PATTERN = '^\};'

# Strings relating to files we'll need to read.
# data_type.cc is where the DataTypeInfoMap is
# entity_specifics.proto is where the proto definitions for DataTypes are.
ENTITY_SPECIFICS_PROTO_FILE_PATH = './protocol/entity_specifics.proto'
ENTITY_SPECIFICS_PROTO_FILE_NAME = 'entity_specifics.proto'
DATA_TYPE_FILE_NAME = 'data_type.cc'

SYNC_SOURCE_FILES = (r'^components[\\/]sync[\\/].*\.(cc|h)$',)

def CheckDataTypeInfoMap(input_api, output_api, data_type_file):
  """Checks the kDataTypeInfoMap in data_type.cc follows conventions.
  Checks that the kDataTypeInfoMap follows the below rules:
    1) The data type string should match the data type name, but with
       only the first letter capitalized and spaces instead of underscores.
    2) The root tag should be the same as the data type but all lowercase.
    3) The notification type should match the proto message name.
    4) No duplicate data across data types.
   Args:
    input_api: presubmit_support InputApi instance
    output_api: presubmit_support OutputApi instance
    data_type_file: AffectedFile object where the DataTypeInfoMap is
  Returns:
    A (potentially empty) list PresubmitError objects corresponding to
    violations of the above rules.
  """
  accumulated_problems = []
  map_entries = ParseDataTypeEntries(
    input_api, data_type_file.AbsoluteLocalPath())
  # If any line of the map changed, we check the whole thing since
  # definitions span multiple lines and there are rules that apply across
  # all definitions e.g. no duplicated field values.
  check_map = False
  for line_num, _ in data_type_file.ChangedContents():
    for map_entry in map_entries:
      if line_num in map_entry.affected_lines:
        check_map = True
        break

  if not check_map:
    return []
  proto_field_definitions = ParseEntitySpecificsProtoFieldIdentifiers(
    input_api, os.path.abspath(ENTITY_SPECIFICS_PROTO_FILE_PATH))
  accumulated_problems.extend(
    CheckNoDuplicatedFieldValues(output_api, map_entries))

  for map_entry in map_entries:
    entry_problems = []
    entry_problems.extend(
      CheckNotificationTypeMatchesProtoMessageName(
        output_api, map_entry, proto_field_definitions))
    entry_problems.extend(CheckRootTagNotInBlocklist(output_api, map_entry))

    if map_entry.data_type not in EXCEPTION_DATA_TYPES:
      entry_problems.extend(
        CheckDataTypeStringMatchesDataType(output_api, map_entry))
      entry_problems.extend(
        CheckRootTagMatchesDataType(output_api, map_entry))

    if len(entry_problems) > 0:
      accumulated_problems.extend(entry_problems)

  return accumulated_problems


class DataTypeEnumEntry(object):
  """Class that encapsulates a DataTypeInfo definition in data_type.cc.
  Allows access to each of the named fields in the definition and also
  which lines the definition spans.
  Attributes:
    data_type: entry's DataType enum value
    notification_type: data type's notification string
    root_tag: data type's root tag
    data_type_string: string corresponding to the DataType
    field_number: proto field number
    histogram_val: value identifying DataType in histogram
    affected_lines: lines in data_type.cc that the definition spans
  """
  def __init__(self, entry_strings, affected_lines):
    (data_type, notification_type, root_tag, data_type_string,
          field_number, histogram_val) = entry_strings
    self.data_type = data_type
    self.notification_type = notification_type
    self.root_tag = root_tag
    self.data_type_string = data_type_string
    self.field_number = field_number
    self.histogram_val = histogram_val
    self.affected_lines = affected_lines


def ParseDataTypeEntries(input_api, data_type_cc_path):
  """Parses data_type_cc_path for DataTypeEnumEntries
  Args:
    input_api: presubmit_support InputAPI instance
    data_type_cc_path: path to file containing the DataTypeInfo entries
  Returns:
    A list of DataTypeEnumEntry objects read from data_type.cc.
    e.g. ('AUTOFILL_WALLET_METADATA', 'WALLET_METADATA',
      'autofill_wallet_metadata', 'Autofill Wallet Metadata',
      'sync_pb::EntitySpecifics::kWalletMetadataFieldNumber', '35',
      [63, 64, 65])
  """
  file_contents = input_api.ReadFile(data_type_cc_path)
  start_pattern = input_api.re.compile(DATA_TYPE_START_PATTERN)
  end_pattern = input_api.re.compile(DATA_TYPE_END_PATTERN)
  results, definition_strings, definition_lines = [], [], []
  inside_enum = False
  current_line_number = 0
  for line in file_contents.splitlines():
    current_line_number += 1
    if line.strip().startswith('//'):
      # Ignore comments.
      continue
    if start_pattern.match(line):
      inside_enum = True
      continue
    if inside_enum:
      if end_pattern.match(line):
        break
      line_entries = line.strip().strip('{},').split(',')
      definition_strings.extend([entry.strip('" ') for entry in line_entries])
      definition_lines.append(current_line_number)
      if line.endswith('},'):
        results.append(DataTypeEnumEntry(definition_strings, definition_lines))
        definition_strings = []
        definition_lines = []
  return results


def ParseEntitySpecificsProtoFieldIdentifiers(input_api, proto_path):
  """Parses proto field identifiers from the EntitySpecifics definition.
  Args:
    input_api: presubmit_support InputAPI instance
    proto_path: path to the file containing the proto field definitions
  Returns:
    A dictionary of the format {'SyncDataType': 'field_identifier'}
    e.g. {'AutofillSpecifics': 'autofill'}
  """
  proto_field_definitions = {}
  proto_file_contents = input_api.ReadFile(proto_path).splitlines()
  start_pattern = input_api.re.compile(PROTO_DEFINITION_START_PATTERN)
  end_pattern = input_api.re.compile(PROTO_DEFINITION_END_PATTERN)
  in_proto_def = False
  split_proto_line = []
  for line in proto_file_contents:
    if start_pattern.match(line):
      in_proto_def = True
      continue
    if in_proto_def:
      if end_pattern.match(line):
        break
      # Clean up last line on the end of the definition.
      if len(split_proto_line) > 0 and (';' in split_proto_line[-1]):
        split_proto_line.clear()
      line = line.strip()
      # Ignore comments.
      if '//' in line:
        continue
      split_proto_line.extend(line.split(' '))
      # Ignore lines that don't contain definitions.
      if len(split_proto_line) < 3:
        continue

      field_typename = split_proto_line[0]
      field_identifier = split_proto_line[1]
      proto_field_definitions[field_typename] = field_identifier
  return proto_field_definitions

def StripTrailingS(string):
  return string.rstrip('sS')


def IsTitleCased(string):
  return all([s[0].isupper() for s in string.split(' ')])


def FormatPresubmitError(output_api, message, affected_lines):
  """ Outputs a formatted error message with filename and line number(s).
  """
  if len(affected_lines) > 1:
    message_including_lines = 'Error at lines %d-%d in data_type.cc: %s' %(
      affected_lines[0], affected_lines[-1], message)
  else:
    message_including_lines = 'Error at line %d in data_type.cc: %s' %(
      affected_lines[0], message)
  return output_api.PresubmitError(message_including_lines)


def CheckNotificationTypeMatchesProtoMessageName(
  output_api, map_entry, proto_field_definitions):
  """Check that map_entry's notification type matches entity_specifics.proto.
  Verifies that the notification_type matches the name of the field defined
  in the entity_specifics.proto by looking it up in the proto_field_definitions
  map.
  Args:
    output_api: presubmit_support OutputApi instance
    map_entry: DataTypeEnumEntry instance
    proto_field_definitions: dict of proto field types and field names
  Returns:
    A potentially empty list of PresubmitError objects corresponding to
    violations of the above rule
  """
  if map_entry.field_number == '-1':
    return []
  proto_message_name = proto_field_definitions[
    FieldNumberToPrototypeString(map_entry.field_number)]
  if map_entry.notification_type.lower() != proto_message_name:
    return [
      FormatPresubmitError(
        output_api,'In the construction of DataTypeInfo: notification type'
        ' "%s" does not match proto message'
        ' name defined in entity_specifics.proto: ' '"%s"' %
        (map_entry.notification_type, proto_message_name),
        map_entry.affected_lines)]
  return []


def CheckNoDuplicatedFieldValues(output_api, map_entries):
  """Check that map_entries has no duplicated field values.
  Verifies that every map_entry in map_entries doesn't have a field value
  used elsewhere in map_entries, ignoring special values ("" and -1).
  Args:
    output_api: presubmit_support OutputApi instance
    map_entries: list of DataTypeEnumEntry objects to check
  Returns:
    A list PresubmitError objects for each duplicated field value
  """
  problem_list = []
  field_value_sets = [set() for i in range(MAP_ENTRY_FIELD_COUNT)]
  for map_entry in map_entries:
    field_values = [
      map_entry.data_type, map_entry.notification_type,
      map_entry.root_tag, map_entry.data_type_string,
      map_entry.field_number, map_entry.histogram_val]
    for i in range(MAP_ENTRY_FIELD_COUNT):
      field_value = field_values[i]
      field_value_set = field_value_sets[i]
      if field_value in field_value_set:
        problem_list.append(
          FormatPresubmitError(
            output_api, 'Duplicated field value "%s"' % field_value,
            map_entry.affected_lines))
      elif len(field_value) > 0 and field_value != '-1':
        field_value_set.add(field_value)
  return problem_list


def CheckDataTypeStringMatchesDataType(output_api, map_entry):
  """Check that map_entry's data_type_string matches DataType.
  Args:
    output_api: presubmit_support OutputApi instance
    map_entry: DataTypeEnumEntry object to check
  Returns:
    A list of PresubmitError objects for each violation
  """
  problem_list = []
  expected_data_type_string = map_entry.data_type.lower().replace('_', ' ')
  if (StripTrailingS(expected_data_type_string) !=
    StripTrailingS(map_entry.data_type_string.lower())):
    problem_list.append(
      FormatPresubmitError(
        output_api,'data type string "%s" does not match data type.'
        ' It should be "%s"' % (
          map_entry.data_type_string, expected_data_type_string.title()),
        map_entry.affected_lines))
  if not IsTitleCased(map_entry.data_type_string):
    problem_list.append(
      FormatPresubmitError(
        output_api,'data type string "%s" should be title cased' %
        (map_entry.data_type_string), map_entry.affected_lines))
  return problem_list


def CheckRootTagMatchesDataType(output_api, map_entry):
  """Check that map_entry's root tag matches DataType.
  Args:
    output_api: presubmit_support OutputAPI instance
    map_entry: DataTypeEnumEntry object to check
  Returns:
    A list of PresubmitError objects for each violation
  """
  expected_root_tag = map_entry.data_type.lower()
  if (StripTrailingS(expected_root_tag) !=
    StripTrailingS(map_entry.root_tag)):
    return [
      FormatPresubmitError(
        output_api,'root tag "%s" does not match data type. It should '
        'be "%s"' % (map_entry.root_tag, expected_root_tag),
        map_entry.affected_lines)]
  return []

def CheckRootTagNotInBlocklist(output_api, map_entry):
  """ Checks that map_entry's root isn't a blocklisted string.
  Args:
    output_api: presubmit_support OutputAPI instance
    map_entry: DataTypeEnumEntry object to check
  Returns:
    A list of PresubmitError objects for each violation
  """
  if map_entry.root_tag in BLOCKLISTED_ROOT_TAGS:
    return [FormatPresubmitError(
        output_api,'root tag "%s" is a blocklisted root tag'
        % (map_entry.root_tag), map_entry.affected_lines)]
  return []


def FieldNumberToPrototypeString(field_number):
  """Converts a field number enum reference to an EntitySpecifics string.
  Converts a reference to the field number enum to the corresponding
  proto data type string.
  Args:
    field_number: string representation of a field number enum reference
  Returns:
    A string that is the corresponding proto field data type. e.g.
    FieldNumberToPrototypeString('EntitySpecifics::kAppFieldNumber')
    => 'AppSpecifics'
  """
  return field_number.replace(FIELD_NUMBER_PREFIX, '').replace(
    'FieldNumber', 'Specifics')

def CheckChangeLintsClean(input_api, output_api):
  source_filter = lambda x: input_api.FilterSourceFile(
    x, files_to_check=SYNC_SOURCE_FILES, files_to_skip=None)
  return input_api.canned_checks.CheckChangeLintsClean(
      input_api, output_api, source_filter, lint_filters=[], verbose_level=1)

def CheckChanges(input_api, output_api):
  results = []
  results += CheckChangeLintsClean(input_api, output_api)

  proto_file_changed = False
  proto_visitors_changed = False

  for f in input_api.AffectedFiles():
    if (f.LocalPath().endswith(DATA_TYPE_FILE_NAME) or
        f.LocalPath().endswith(ENTITY_SPECIFICS_PROTO_FILE_NAME)):
      results += CheckDataTypeInfoMap(input_api, output_api, f)

    if (f.LocalPath().endswith('.proto')):
      proto_file_changed = True
    if (f.LocalPath().endswith(os.path.sep + 'proto_visitors.h')):
      proto_visitors_changed = True

  if proto_file_changed and not proto_visitors_changed:
    results.append(
      output_api.PresubmitPromptWarning(
        'You changed proto files, but didn\'t change proto_visitors.h'))

  return results

def CheckChangeOnUpload(input_api, output_api):
  return CheckChanges(input_api, output_api)

def CheckChangeOnCommit(input_api, output_api):
  return CheckChanges(input_api, output_api)
