<?php 

/**
 * 
 * Remove a pattern from the database.
 * 
 * @param mixed $pid the id of the pattern to remove
 */
function patterns_db_remove_pattern($pid) {
  
  $del = db_delete('patterns')
    ->condition('pid', $pid)
    ->execute();
  

  return ($del==1) ? TRUE : FALSE;
}

/**
 * Writes the pattern metadata (and the actual pattern) to the database.
 * 
 * @param array $pattern
 * @param mixed $path the new path where the file has just been saved or read from
 * @param mixed $name the name of the pattern file without the extension
 */
function patterns_db_save_pattern($pattern, $path = '', $name = '') {

  // Try to parse the array in two different formats.
  if (isset($pattern['info'])) {
    $title = $pattern['info']['title'];
    $description = $pattern['info']['description'];
    $author = $pattern['info']['author'];
  }
  else {
    
    if (isset($pattern['title'])) {
       $title = $pattern['title'];
    }
    
    if (isset($pattern['description'])) {
       $description = $pattern['description'];
    }
    
    if (isset($pattern['author'])) {
       $author = $pattern['author'];
    }
    
  }
  
  $pattern_data = db_query("SELECT pid, file, updated FROM {patterns} WHERE name = :name", array('name' => $name))->fetchAssoc();
  
  // Pattern already in the db, data needs to be updated
  if (!empty($pattern_data)) {
    
    $pid = $pattern_data['pid'];
    $updated = $pattern_data['updated']; //$updated = (db_query("SELECT updated FROM {patterns} WHERE pid = :d", array('d' => $pid))->fetchField());
    $file = $pattern_data['file'];
    
    // Change of location
    if ($path != $file) {
      $query_params = array('file' => $path, 'updated' => time(), 'pid' => $pid);
      db_query("UPDATE {patterns} SET file = :file, updated = :updated WHERE pid = :pid", $query_params);
    }
    
    //
    if (($new_updated = filemtime($path)) > $updated) {
    $query_params = array('pattern' => serialize($pattern), 'title' => $title, 'file' => $path, 'updated' => $new_updated, 'descr' => $description, 'pid' => $pid);
      db_query("UPDATE {patterns} SET pattern = :pattern, title = :title, file = :file, updated = :updated, description = :descr WHERE pid = :pid", $query_params); 
    }
    else {
      $query_params = array('pattern' => serialize($pattern), 'title' => $title, 'file' => $path, 'descr' => $description, 'pid' => $pid);
      db_query("UPDATE {patterns} SET pattern = :pattern, title = :title, file = :file, description = :descr WHERE pid = :pid", $query_params); 
    }
  }
  
  // It is a new Pattern
  else {
   // TODO: time() ?= REQUEST_TIME
    $query_params = array('name' => $name, 'pattern' => serialize($pattern), 'title' => $title, 'file' => $path, 'descr' => $description, 'time' => time());
    db_query("INSERT INTO {patterns} (name, status, file, updated, enabled, title, description, pattern) VALUES ( :name, 0, :file, :time, 0, :title, :descr, :pattern)", $query_params);
  }
  
  return TRUE;
}
