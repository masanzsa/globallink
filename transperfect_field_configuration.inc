<?php

/**
 * @file
 * Transperfect_field_configuration.inc is a file that contains most functions
 * needed on the Field Configuration UI.
 */

function transperfect_field_page() {
  return drupal_get_form('transperfect_field');
}

function transperfect_field() {
  module_load_include('inc', 'transperfect', 'transperfect');

  $selected_value = '';

  if (!empty($_SESSION['transperfect_selected_content_type'])) {
    $selected_value = $_SESSION['transperfect_selected_content_type'];
  }

  $form = array();

  $n_arr = array('' => t('--Select Content Type--'));
  $t_arr = transperfect_get_translatable_node_types_and_names();

  $node_types = $n_arr + $t_arr;

  $form['#attributes']['class'][] = 'transperfect-field';

  $form['select_field'] = array(
    '#type' => 'container',
    '#attributes' => array('class' => array('container-inline')),
  );

  $form['select_field']['select_type'] = array(
    '#type' => 'select',
    '#title' => t('Content Type'),
    '#options' => $node_types,
    '#default_value' => $selected_value,
    '#attributes' => array(
      'class' => array('transperfect-field-select-type'),
    ),
    '#field_suffix' => '&nbsp;&nbsp;&nbsp;&nbsp;',
  );

  $form['select_field']['go'] = array(
    '#type' => 'submit',
    '#value' => t('Go'),
    '#attributes' => array('style' => 'display: none;'),
  );

  if ($selected_value != '') {
    $disabled = FALSE;

    $f_arr = array('[all]' => t('All'));
    $p_arr = transperfect_get_pending_fields($selected_value);

    $pending_fields = $f_arr + $p_arr;

    if (count($pending_fields) == 1) {
      $pending_fields = array('' => '--Select--');
      $disabled = TRUE;
    }

    $form['select_field']['pending_fields'] = array(
      '#type' => 'select',
      '#title' => '&nbsp;&nbsp;Fields:&nbsp;&nbsp;',
      '#options' => $pending_fields,
      '#field_suffix' => '&nbsp;&nbsp;&nbsp;&nbsp;',
    );

    $form['select_field']['add_field'] = array(
      '#type' => 'submit',
      '#value' => t('Add'),
      '#disabled' => $disabled,
    );

    $form['select_field']['br_markup'] = array(
      '#type' => 'markup',
      '#markup' => '<BR/><BR/>',
    );

    $header = array(
      'field_label' => array(
        'field' => 'field_label',
        'data' => t('Field Label'),
      ),
      'field_name' => array(
        'field' => 'field_name',
        'data' => t('Field Name'),
      ),
      'field_type' => array(
        'field' => 'field_type',
        'data' => t('Field Type'),
      ),
    );

    $query = db_select('transperfect_field_config', 'tf')
    ->fields('tf')
    ->condition('content_type', $selected_value, '=')
    ->extend('TableSort') // Sorting Extender
    ->orderByHeader($header) // Field to sort on is picked from $header
    ;

    $results = $query->execute();

    $count = 0;

    $rows = array();

    $default_values = array();

    $field_types = field_info_field_types();

    foreach ($results as $item) {
      if ($item->field_type == 'field_collection') {
        continue;
      }

      $count++;

      $rows[$item->fid] = array(
        'field_label' => $item->field_label,
        'field_name' => $item->field_name,
        'field_type' => isset($field_types[$item->field_type]) ? $field_types[$item->field_type]['label'] : $item->field_type,
      );

      if ($item->translatable == 1) {
        $default_values[$item->fid] = 1;
      }
      else {
        $default_values[$item->fid] = 0;
      }
    }

    $form['field-heading'] = array(
      '#type' => 'container',
    );

    $form['field-heading'][] = array(
      '#markup' => '<em>' . t('Check all the fields that you wish to translate for this content type.') . '</em><br>',
    );

    $form['table'] = array(
      '#type' => 'tableselect',
      '#header' => $header,
      '#options' => $rows,
      '#empty' => t('No items available'),
      '#default_value' => $default_values,
    );

    if (transperfect_content_type_workbench_enabled($selected_value)) {
      if ($states = workbench_moderation_state_labels()) {
        $mod_key = '';
        $current_value = variable_get('transperfect_moderation_' . $selected_value, FALSE);

        if ($current_value) {
          $states_value = $states[$current_value];
          unset($states[$current_value]);

          $new_value = array(
            $mod_key => t('Current: @state', array(
              '@state' => $states_value,
            )
          ));

          $states = array_merge($new_value, $states);
        }

        $form['state'] = array(
          '#title' => t('Moderation state (after import)'),
          '#type' => 'select',
          '#options' => $states,
          '#default_value' => $mod_key,
        );
      }
    }

    if ($count > 0) {
      $form['submit_field_update'] = array(
        '#type' => 'submit',
        '#value' => t('Update'),
      );
    }
  }

  return $form;
}

function transperfect_field_submit($form, &$form_state) {
  $op = isset($form_state['values']['op']) ? $form_state['values']['op'] : '';

  switch ($op) {
    case t('Go'):
      $_SESSION['transperfect_selected_content_type'] = $form_state['values']['select_type'];

      break;
    case t('Update'):
      $type = $form_state['values']['select_type'];

      $fids = array_filter($form_state['values']['table']);

      transperfect_update_gl_field_config($type, $fids);

      $state = isset($form_state['input']['state']) ? $form_state['input']['state'] : FALSE;

      if ($state) {
        variable_set('transperfect_moderation_' . $type, $state);
      }

      drupal_set_message(t('Field configurations have been saved successfully.'));

      break;
    case t('Add'):
      $field = $form_state['values']['pending_fields'];

      transperfect_insert_gl_field_config($field, $_SESSION['transperfect_selected_content_type']);
      drupal_set_message(t('Field configurations have been added successfully.'));

      break;
  }
}

function transperfect_update_gl_field_config($type, $fids) {
  // First update everything to 0 for this type
  db_update('transperfect_field_config')
  ->fields(array('translatable' => 0))
  ->condition('content_type', $type, '=')
  ->execute();

  foreach ($fids as $fid) {
    db_update('transperfect_field_config')
    ->fields(array('translatable' => 1))
    ->condition('fid', $fid, '=')
    ->execute();
  }
}

function transperfect_insert_gl_field_config($field_name, $content_type) {
  $p_arr = transperfect_get_pending_fields($content_type);

  if ($field_name == '[all]') {
    $f_keys = array_keys($p_arr);

    foreach ($f_keys as $f_key) {
      if ($f_key == '[all]') {
        continue;
      }

      if ($f_key != 'title' && $f_key != 'metatags') {
        $field = field_info_field($f_key);

        switch ($field['type']) {
          case 'list_boolean':
          case 'file':
          case 'taxonomy_term_reference':
          case 'field_collection':
            break;
          default:
            db_insert('transperfect_field_config')
            ->fields(array(
              'content_type' => $content_type,
              'entity_type' => 'node',
              'bundle' => $content_type,
              'field_name' => $f_key,
              'field_type' => $field['type'],
              'field_label' => $p_arr[$f_key],
              'translatable' => 1,
            ))->execute();
        }
      }
      else {
        db_insert('transperfect_field_config')
        ->fields(array(
          'content_type' => $content_type,
          'entity_type' => 'node',
          'bundle' => $content_type,
          'field_name' => $f_key,
          'field_type' => 'text',
          'field_label' => $p_arr[$f_key],
          'translatable' => 1
        ))->execute();
      }
    }

    if (module_exists('field_collection')) {
      transperfect_insert_fc_fields($content_type);
    }
  }
  else {
    if ($field_name != 'title' && $field_name != 'metatags') {
      $field = field_info_field($field_name);

      switch ($field['type']) {
        case 'list_boolean':
        case 'file':
        case 'taxonomy_term_reference':
          break;
        case 'field_collection':
          transperfect_insert_fc('node', $field_name, $content_type, $content_type);

          break;
        default:
          db_insert('transperfect_field_config')
          ->fields(array(
            'content_type' => $content_type,
            'entity_type' => 'node',
            'bundle' => $content_type,
            'field_name' => $field_name,
            'field_type' => $field['type'],
            'field_label' => $p_arr[$field_name],
            'translatable' => 1,
          ))->execute();
      }
    }
    else {
      db_insert('transperfect_field_config')
      ->fields(array('content_type' => $content_type,
        'entity_type' => 'node',
        'bundle' => $content_type,
        'field_name' => $field_name,
        'field_type' => 'text',
        'field_label' => $p_arr[$field_name],
        'translatable' => 1,
      ))->execute();
    }
  }
}

function transperfect_insert_fc_fields($node_type) {
  $field_arr = field_info_instances('node', $node_type);

  $keys = array_keys($field_arr);

  foreach ($keys as $field_name) {
    $field_info = field_info_field($field_name);

    if ($field_info['type'] != 'field_collection') {
      continue;
    }

    db_insert('transperfect_field_config')
    ->fields(array(
      'content_type' => $node_type,
      'entity_type' => 'node',
      'bundle' => $node_type,
      'field_name' => $field_name,
      'field_type' => $field_info['type'],
      'field_label' => $field_arr[$field_name]['label'],
      'translatable' => 0,
    ))->execute();

    $fc_field_infos = field_info_instances('field_collection_item');

    if (isset($fc_field_infos) && isset($fc_field_infos[$field_name]) && is_array($fc_field_infos[$field_name])) {
      $fc_items = array_keys($fc_field_infos[$field_name]);

      foreach ($fc_items as $fc_item) {
        transperfect_insert_fc_item_fields($node_type, $field_name, $fc_item);
      }
    }
  }
}

function transperfect_insert_fc_item_fields($content_type, $parent_field_name, $field_name) {
  $fc_field_info = field_info_field($field_name);
  $fc_field_instance = field_info_instance('field_collection_item', $field_name, $parent_field_name);

  switch ($fc_field_info['type']) {
    case 'list_boolean':
    case 'file':
    case 'taxonomy_term_reference':
      break;
    case 'field_collection':
      db_insert('transperfect_field_config')
      ->fields(array(
        'content_type' => $content_type,
        'entity_type' => 'field_collection_item',
        'bundle' => $parent_field_name,
        'field_name' => $field_name,
        'field_type' => $fc_field_info['type'],
        'field_label' => $fc_field_instance['label'],
        'translatable' => 0,
      ))->execute();

      $fc_field_infos = field_info_instances('field_collection_item');

      if (isset($fc_field_infos) && isset($fc_field_infos[$field_name]) && is_array($fc_field_infos[$field_name])) {
        $fc_items = array_keys($fc_field_infos[$field_name]);

        foreach ($fc_items as $fc_item) {
          transperfect_insert_fc_item_fields($content_type, $field_name, $fc_item);
        }
      }

      break;
    default:
      $translatable = 1;

      db_insert('transperfect_field_config')
      ->fields(array(
        'content_type' => $content_type,
        'entity_type' => 'field_collection_item',
        'bundle' => $parent_field_name,
        'field_name' => $field_name,
        'field_type' => $fc_field_info['type'],
        'field_label' => $fc_field_instance['label'],
        'translatable' => $translatable,
      ))->execute();
  }
}