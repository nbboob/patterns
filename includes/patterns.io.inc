<?php
/**
 * @file
 * Functions related to input/output operations.
 */

/**
 * Loads and validates a YAML pattern file.
 *
 * @param string $path The path of the pattern file.
 * @param bool $local
 *   (optional) TRUE if the pattern file is local. Defaults to TRUE.
 *
 * @return array|bool
 *   The pattern structure, or FALSE.
 */
function patterns_load_yaml($path, $local = TRUE) {
  if ($local && !file_exists($path)) {
    return FALSE;
  }

  include_once libraries_get_path('spyc') . '/spyc.php';
  $pattern = Spyc::YAMLLoad($path);

  if (!patterns_validate_pattern($pattern)) {
    return FALSE;
  }

  return $pattern;
}

//function patterns_load_string_yaml($source) {
//  // loading yaml from source doesn't preserve line breaks
//  // so we need to save it as a file first
//  $path = file_directory_temp() . '/import.yaml';
//  file_save_data($source, $path, FILE_EXISTS_REPLACE);
//  $pattern = patterns_load_yaml($path);
//  unlink($path);
//
//  return $pattern;
//}

/**
 * Loads an XML pattern file without validating.
 *
 * @param string $path The path of the pattern file.
 * @param bool $local
 *   (optional) TRUE if the pattern file is local. Defaults to TRUE.
 *
 * @return array|bool
 *   The pattern structure, or FALSE.
 */
function patterns_load_xml($path, $local = TRUE) {
  if ($local && !file_exists($path)) {
    return FALSE;
  }

  if (!$xml = file_get_contents($path)) {
    return FALSE;
  }

  return patterns_load_string_xml($xml);
}

/**
 * @TODO
 */
function patterns_load_string_xml($source) {
  $pattern = patterns_from_source($source);
  if (empty($pattern) || $pattern['tag'] != 'pattern') {
    return FALSE;
  }

  // Rearrange the data in a nice way for each component.
  // Make sure actions are processed differently so order is preserved.
  $pattern = patterns_rearrange_data($pattern);

  foreach ($pattern as $key => $values) {
    $pattern[$values['tag']] = $values;
    unset($pattern[$values['tag']]['tag']);
    unset($pattern[$key]);
  }

  if (!patterns_validate_pattern($pattern)) {
    return FALSE;
  }

  return $pattern;
}

/**
 * Loads and validates a PHP pattern file.
 *
 * @param string $path The path of the pattern file.
 * @param bool $local
 *   (optional) TRUE if the pattern file is local. Defaults to TRUE.
 *
 * @return array|bool
 *   The pattern structure, or FALSE.
 */
function patterns_load_php($path, $local = TRUE) {
  if ($local && !file_exists($path)) {
    return FALSE;
  }
  $pattern = array();

  include($path);
  // That should have declared a 'pattern' into current scope.

  if (!patterns_validate_pattern($pattern)) {
    // TODO: t()
    trigger_error("Failed to evaluate a useful pattern from the input file $path. Pattern did not validate. May have been invalid syntax. ", E_USER_WARNING);
    return FALSE;
  }
  return $pattern;
}

/**
 * Creates a pattern from an XML data source.
 *
 * @param string $xml The XML string of the data source.
 *
 * @return array|bool
 *   The pattern structure, or FALSE.
 */

function patterns_from_source($xml) {
  $parse = drupal_xml_parser_create($xml);
  xml_parser_set_option($parse, XML_OPTION_CASE_FOLDING, 0);

  if (!xml_parse_into_struct($parse, $xml, $vals, $index)) {
    return FALSE;
  }

  // Create a multi-dimensional array representing the XML structure.
  $pattern = current(_patterns_parse_tag($vals));

  return $pattern;
}

function patterns_rearrange_data($pattern) {
  foreach ($pattern as $key => $value) {
    if (is_string($key)) {
      unset($pattern[$key]);
    }
    else {
      if ($value['tag'] == 'actions') {
        $pattern[$key] = patterns_rearrange_data($value);
        $pattern[$key]['tag'] = 'actions';
      }
      else {
        $pattern[$key] = _patterns_rearrange_data($value);
      }
    }
  }

  return $pattern;
}

function _patterns_rearrange_data($data, $parent = '') {
  $numeric = array();
  $count = 0;

  foreach ($data as $key => $value) {
    if (isset($value['value'])) {
      if ($value['value'] == 'false') {
        $value['value'] = FALSE;
      }
      elseif ($value['value'] == 'true') {
        $value['value'] = TRUE;
      }
    }

    if (is_numeric($key) && is_array($value) && count($value) == 2 && isset($value['tag']) && isset($value['value'])) {
      unset($data[$key]);
      if (isset($data[$value['tag']])) {
        $numeric[] = $value['tag'];
        $data[$count++] = $data[$value['tag']];
        $data[$count++] = $value['value'];
        unset($data[$value['tag']]);
      }
      elseif (in_array($value['tag'], $numeric)) {
        $data[$count++] = $value['value'];
      }
      else {
        $data[$value['tag']] = $value['value'];
      }
    }
    elseif (is_numeric($key)) {
      $tag = $value['tag'];
      unset($value['tag']);
      $data[$tag][] = _patterns_rearrange_data($value, $tag);
      unset($data[$key]);
    }
  }

  foreach ($data as $key => $value) {
    if (is_array($value) && count($value) == 1 && $value[0]) {
      $data[$key] = $data[$key][0];
    }
  }

  // This workaround enables us to define numeric keys in XML by
  // prefixing the number with single character. E.g <n0>value</n0>
  // will result in 0 => 'value' (first character of the key will be removed).
  if (isset($data['_numeric_keys'])) {
    unset($data['_numeric_keys']);
    foreach ($data as $key => $value) {
      $data[substr($key, 1)] = $value;
      unset($data[$key]);
    }
  }

  return $data;
}

/**
 * Menu callback, returns source code of the requested pattern
 * if the pattern is public.
 *
 * @param integer $pid
 *   The ID of the Pattern to be displayed.
 */
function patterns_get_source($pid) {
  if (!is_numeric($pid)) {
    exit;
  }

  $pattern = patterns_get_pattern($pid);

  // Make sure pattern is public (published).
  if (!$pattern->public) {
    exit;
  }

  $content_type = 'text/plain';
  if (substr($pattern->file, -4) == '.xml') {
    $content_type = 'text/xml';
  }

  drupal_add_http_header('Content-Type', $content_type . '; charset=utf-8');
  print file_get_contents($pattern->file);
  exit;
}

/**
 * Loads the Patterns handlers (component) from the filesystem,
 * if they are not already loaded.
 */
function patterns_load_components() {
  static $loaded = FALSE;

  if ($loaded) {
    return;
  }

  // Get list of directories to scan for components.
  $paths = module_invoke_all('patterns_components');

  foreach ($paths as $path) {
    foreach (file_scan_directory($path, '/\.inc$/') as $file) {
      include_once $file->uri;
    }
  }

  $loaded = TRUE;
}

/**
 * Loads the available patterns.
 *
 * Checks inside the directories defined by patterns_paths().
 * @see patterns_paths
 *
 * @param $reset
 *
 * @return array The array of available patterns.
 */
function patterns_get_patterns($reset = TRUE) {
  patterns_load_components();

  if ($reset || !variable_get('patterns_loaded', FALSE)) {
    // Get a listing of enabled patterns.
    $enabled = array();

    $priority = array();
    $errors = array();

    // Get list of directories to scan for patterns.
    $patterns_paths = patterns_paths();

    // Get valid file extensions.
    $mask = '/.\.(' . implode('|', patterns_file_types()) . ')$/';

    // Prepare list of files/folders to be excluded.
    // 'enabled' - Don't save enabled pattern backups.
    // @TODO: extend these.
    $no_mask = array('.', '..', 'CVS', '.svn', 'enabled');

    foreach ($patterns_paths as $path) {
      foreach (file_scan_directory($path, $mask, $no_mask) as $file) {
        // Can't update existing patterns that are enabled.
        if (in_array($file->filename, $enabled) || in_array($file->name, $priority)) {
          continue;
        }

        $priority[] = $file->name;

        // Choose appropriate function based on the file extension: YAML or XML.
        $func = 'patterns_load_' . substr($file->filename, strlen($file->name) + 1);

        // Load and save pattern.
        if ($pattern = $func($file->uri)) {
          patterns_db_save_pattern($pattern, $file->uri, $file->name);
        }
        else {
          $errors[] = $file->filename;
        }
      }
    }

    variable_set('patterns_loaded', time());
  }


  $result = db_query('SELECT * FROM {patterns}');


  $messages = array();
  $patterns = array();

  foreach ($result as $pattern) {
    // Skip pattern if its file is missing.
    if (!is_file($pattern->file)) {
      continue;
    }

    // Skip pattern if loading failed and report that to the user.
    if (in_array($pattern->file, $errors)) {
      $messages[] = t("Pattern couldn't be loaded from the file '%file'", array('%file' => $pattern->file));
      continue;
    }

    $patterns[$pattern->pid] = $pattern;
    $data = unserialize($pattern->pattern);
    $patterns[$pattern->pid]->pattern = $data;
    $patterns[$pattern->pid]->info = $data['info'];
  }

  if (!empty($messages)) {
    drupal_set_message(implode('<br/>', $messages) . '<br/>' . t('Make sure that above file(s) are readable and contain valid data.'), 'error');
  }

  return $patterns;
}

function patterns_io_get_patterns_service() {
  return 'Ciao'; // TODO
}

/**
 * Checks if a .htaccess file exists to prevent downloads of pattern files.
 */
function _patterns_check_file_dir() {
  return FALSE;
  // Ste: was
  //$path = file_create_path(variable_get('patterns_save_xml', 'patterns'));

  // real_path(public) returns the path to the files dir
  $path = drupal_realpath('public://') . "/" . variable_get('patterns_save_xml', 'patterns');

  if (!is_file($path . '/.htaccess')) {
    $content = '# Prevent downloading site patterns
<FilesMatch "\.xml$">
  Order allow,deny
</FilesMatch>
';
    file_save_data($content, $path . '/.htaccess');
  }
}

/**
 * Checks if the patterns directory exist and is writable.
 */
function _patterns_is_patterns_dir_ready() {
  $patterns_files_dir = '/srv/http/dr7/sites/default/files' . "/" . variable_get('patterns_save_file', 'patterns'); // TODO: move this out for performance
  return (file_prepare_directory($patterns_files_dir)) ? TRUE : FALSE;
}

/**
 * Saves a pattern.
 *
 * Replaces the pattern if it already exists. Produces error messages if the
 * pattern could not be saved.
 *
 * @param string $content The content of the pattern to be saved.
 * @param string $name The name of the pattern file.
 * @param string $format
 *   (optional) The format of the pattern. Defaults to 'yaml'.
 *
 * @return bool
 *   TRUE on success, FALSE otherwise.
 *
 * @TODO: Move $format as an element of pattern, a column in the DB.
 *
 */
function patterns_io_save_pattern($content, $name, $format = 'yaml') {

  // System Patterns directory.
  $dir = drupal_realpath('public://') . "/" . variable_get('patterns_save_file', 'patterns');

  if (file_prepare_directory($dir, TRUE)) {
    $path =  $dir . '/' . $name . '.' . $format;

    if (file_unmanaged_save_data($content, $path, FILE_EXISTS_REPLACE)) {

      // Load and save pattern.
      $load_func = 'patterns_load_' . $format;

      if ($pattern = $load_func($path)) {
        patterns_db_save_pattern($pattern, $path, $name);
        drupal_set_message(t('%name was saved.', array('%name' => $name)));
        // Not for now.
        //_patterns_clear_failed_edit();
      }
      else {
        drupal_set_message(t("Pattern '%name' could not be saved into the database. Make sure edited code is well-formed.", array('%name' => $name)), 'error');
        return FALSE;
      }
    }
    else {
      drupal_set_message(t('Error: file could not be saved.'), 'error');
      return FALSE;
    }
  }
  else {
    drupal_set_message(t('Error: the pattern is not writable. Please check the file system permissions.'), 'error');
    return FALSE;
  }


  return TRUE;
}

function patterns_io_remove_pattern($path) {
  // @TODO
}
