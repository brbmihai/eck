<?php
module_load_include('inc', 'eck', 'eck.classes');
/**
 * @file
 * All of the menu, pages, and forms related to bundle administration.
 */



/**
 * Page call back for the bundle overview table (to see and manipulate all created label of
 * a given type)
 *
 * @param entity_type
 *  (String) entity type
 */
function eck__bundle__list($entity_type) {
  $path = eck__entity_type__path();
  $entity_type = entity_type_load($entity_type);
  
  //Check that the user has permissions to view bundle lists:
  if(eck_access('list', 'bundle'))
  {
    
    $header = array(t('Type'), array('data' => t('Operations'), 'colspan' => '1'));
    $rows = array();
    
    $bundles = Bundle::loadByEntityType($entity_type);
    usort($bundles,'eck_alphabetical_cmp');
    
    foreach ($bundles as $bundle) {
      $bundle_label =  $bundle->label;

      $admin_info = get_bundle_admin_info($entity_type->name, $bundle->name);

      $uri = $admin_info['path'];
      
      $allowed_operations = '';
      //Check that the user has permissions to delete:
      if(eck_access('delete', 'bundle', $bundle)){
        $allowed_operations = l(t('delete'), $uri."/delete");
      }
      
      if(eck_access('list', 'entity')){
        $rows[] = array(l($bundle_label, url($uri, array('absolute' => TRUE))), $allowed_operations);
      }else{
        $rows[] = array($bundle_label, $allowed_operations);
      }
    }
 
    $build['bundle_table'] = array(
      '#theme' => 'table',
      '#header' => $header,
      '#rows' => $rows,
    );
  }



  return $build;
}

/**
 * ADD Entity types.
 *
 * @param $form
 *  Form array provided by the Form API
 * @param $form_state
 *  array provided by the Form API
 * @param entity_type
 *  (String) entity type
 */
function eck__bundle__add($form, &$form_state, $entity_type) {
  $entity_type = entity_type_load($entity_type);
  $form['entity_type'] = array(
    '#type' => 'value',
    '#value' => $entity_type,
  );

  $form['bundle_label'] = array(
    '#type' => 'textfield',
    '#title' => t('Type'),
    '#description' => "A Human readable name for the bundle",
  );

  $form['bundle_name'] = array(
    '#type' => 'machine_name',
    '#required' => FALSE,
    '#machine_name' => array(
      'exists' => '_eck_fake_exists',
      'source' => array('bundle_label'),
    )
  );

  $form['#validate'][] = 'eck__bundle__add_validate';

  $form['submit'] = array(
    '#type' => 'submit',
    '#weight' => 10000,
    '#value' => t('Save'),
  );

  return $form;
}

/**
 * Validation for bundle creation (Make sure this bundle don't exist for thie entity type)
 *
 * @param $form
 *  Form array provided by the Form API
 * @param $form_state
 *  array provided by the Form API
 */
function eck__bundle__add_validate($form, &$form_state) {
  $entity_type = $form_state['values']['entity_type'];
 

  // The type does not have to be unique in the table, but it should be unique
  // to its entity so we will check that here.
  foreach (Bundle::loadByEntityType($entity_type) as $bundle) {
    
    if ($bundle->name == $form_state['values']['bundle_name']) {
      form_set_error('bundle', t("bundle {$bundle->label} already exists for this entity type"));
    }
  }
}

/**
 * Submit function for add form
 *
 * @param $form
 *  Form array provided by the Form API
 * @param $form_state
 *  array provided by the Form API
 */
function eck__bundle__add_submit($form, &$form_state) {
  $path = eck__entity_type__path();
  
  $entity_type = $form_state['values']['entity_type'];
  $bundle_name = $form_state['values']['bundle_name'];
  $bundle_label = $form_state['values']['bundle_label'];

  $bundle = new Bundle();
  $bundle->name = $bundle_name;
  $bundle->label = $bundle_label;
  $bundle->entity_type = $entity_type->name;
  $bundle->save();

  drupal_set_message(t('the %bundle for entity type %entity_type has been saved', 
  array('%bundle' => $bundle_name, '%entity_type' => $entity_type->name)));

  Bundle::loadAll(NULL, TRUE);
  drupal_get_schema(NULL, TRUE);
  entity_info_cache_clear();
  variable_set('menu_rebuild_needed', TRUE);

  $form_state['redirect'] = "{$path}/{$entity_type->name}";
}

/**
 * Delete the bundle of a given entity type
 * @param $entity_type
 *  (EntityType) The entity type of the bundle that will be deleted
 *  the entity type object can be loaded by using EntityType::loadByName($entity_type_name)
 * @param  $bundle
 *  (Bundle) The bundle to be deleted, the Bundle object can be loaded by using 
 *  Bundle::loadByMachingName($bundle_machine_name)
 * 
 * for more information on the EntityType and Bundle classes look at eck.classes.inc
 */
function eck__bundle__delete($entity_type, $bundle){
  //first delete all of the entities of this bundle
  $query = new EntityFieldQuery();
  $query
  ->entityCondition('entity_type', $entity_type->name, '=')
  ->entityCondition('bundle', $bundle->name,'=');
  
  $results = $query->execute();
  
  if(!empty($results)){
    $ids = array_keys($results[$entity_type->name]);
    entity_delete($entity_type->name, $ids);
  }
  //then we delete the bundle (field_instances)
  field_attach_delete_bundle($entity_type->name, $bundle->name);
  
  //and finally we delete the bundle from the eck_type table
  $bundle->delete();
  
  drupal_get_schema(NULL, TRUE);
  variable_set('menu_rebuild_needed', TRUE);
  drupal_flush_all_caches();
  
  drupal_set_message("The bundle '{$bundle->name}' from the entity type '{$entity_type->name}' has been deleted");
}

function eck__bundle__delete_form($form, &$form_state, $entity_type_name, $bundle_name) {
    $path = eck__entity_type__path();
    $entity_type = entity_type_load($entity_type_name);
    $bundle = bundle_load($entity_type_name, $bundle_name);
    
    /*dpm($entity_type);
    dpm($bundle);*/
    
  $form['entity_type'] =
      array(
        '#type' => 'value',
        '#value' => $entity_type,
  );

  $form['bundle'] =
      array(
        '#type' => 'value',
        '#value' => $bundle,
  );

  $form['submit_redirect'] =
  array(
    '#type' => 'value',
    '#value' => "{$path}/{$entity_type->name}",
  );

  $message = t("Are you sure that you want to delete the bundle '{$bundle->name}'");

  $caption = t("All of the data (entities) associated with this bundle
  will be deleted. This action cannot be undone.");

  return confirm_form($form, $message, "{$path}/{$entity_type->name}", $caption, t('Delete'));
}

/**
 * Sumbmit function for the delete functionality
 *
 * @param $form
 *  Form array provided by the Form API
 * @param $form_state
 *  array provided by the Form API
 */
function eck__bundle__delete_form_submit($form, &$form_state) {
  $entity_type = $form_state['values']['entity_type'];
  $bundle = $form_state['values']['bundle'];
  $submit_redirect = $form_state['values']['submit_redirect'];

  eck__bundle__delete($entity_type, $bundle);

  // Ok, lets delete the entity
  $form_state['redirect'] = $submit_redirect;
}

function eck__bundle__field_autocomplete($entity_type, $bundle, $string = ""){
  $field_label = get_bundle_field_label_info($entity_type, $bundle);
  $matches = array();
  
  if($field_label){
    $field = $field_label['field'];
    $language = $field_label['language'];
    $delta = $field_label['delta'];
    $column = $field_label['column'];
    $query = new EntityFieldQuery();
    //TODO: Remove the addmetadata() call below once http://drupal.org/node/997394 is fixed.
    $query->addMetadata('account', user_load(1));

    $query->entityCondition('entity_type', $entity_type->name, '=')
    ->entityCondition('bundle', $bundle->name, '=')
    ->fieldCondition($field, $column, $string, 'CONTAINS');
    
    $results = $query->execute();
    
    $entities = entity_load($entity_type->name, array_keys($results[$entity_type->name]));
    foreach($entities as $id => $entity){
      $matches[$id] = $entity->{$field}[$language][$delta][$column];
    }
  }
  
  drupal_json_output($matches);
}

function get_bundle_field_label_info($entity_type, $bundle){
  
  $info = entity_get_info();
  
  if(array_key_exists('field label', $info[$entity_type->name]['bundles'][$bundle->name])){
    return $info[$entity_type->name]['bundles'][$bundle->name]['field label'];
  }
  else{
    return NULL;
  }
}

/**
 ********** Properties as extra fields ***********
 */

/**
 * Form for managing the extra field settings for a given entity property.
 * 
 * @param $form
 *   The entity property extra field management form. For configuration of extra
 *   field settings like its label.
 * @param $form_state
 *   The current state of the form.
 * @param $entity_type
 *   The machine name of the entity type.
 * @param $bundle_type
 *   The machine name of the bundle type.
 * @param $property_name
 *   The machine name of the entity property being included as an extra field.
 * 
 * @return
 *   An array comprising the form for managing the settings for the extra field.
 */
function eck__manage_extra_field_form($form, &$form_state, $entity_type_name, $bundle_type, $property_name) {
  $entity_type = entity_type_load($entity_type_name);
  $bundle = bundle_load($entity_type_name, $bundle_type);
  drupal_set_title($bundle->config['extra_fields'][$property_name]['form']['label']);

  if (empty($bundle->config['extra_fields'][$property_name]['form'])) {
    $form_state['redirect'] = 'admin/structure/entity-type/' . $entity_type_name . '/' . $bundle->name . '/fields';
    return array();
  }
  $bundle_property_config = $bundle->config['extra_fields'][$property_name]['form'];
  $widget_type = eck_property_info_widget_types($bundle_property_config['widget']['type']);
  
  $form = array (
    '#entity_type' => $entity_type,
    '#bundle' => $bundle,
    '#property_name' => $property_name,
  );
  // Build the configurable property extra field form values.
  $form['label'] = array(
    '#type' => 'textfield',
    '#title' => t('Label'),
    '#default_value' => !empty($bundle_property_config['label']) ? $bundle_property_config['label'] : $property_name,
    '#required' => TRUE,
    '#weight' => -20,
  );
  $form['required'] = array(
    '#type' => 'checkbox',
    '#title' => t('Required field'),
    '#default_value' => !empty($bundle_property_config['required']),
    '#weight' => -10,
  );

  $form['description'] = array(
    '#type' => 'textarea',
    '#title' => t('Help text'),
    '#default_value' => !empty($bundle_property_config['description']) ? $bundle_property_config['description'] : '',
    '#rows' => 5,
    '#description' => t('Instructions to present to the user below this property field on the editing form.<br />Allowed HTML tags: @tags', array('@tags' => _field_filter_xss_display_allowed_tags())),
    '#weight' => -5,
  );
  
  // Include external module file dependency if one is required.
  if (function_exists('drupal_get_path') && $widget_type['file']) {
    form_load_include($form_state, $widget_type['file type'], $widget_type['module'], $widget_type['file']);
  }
  
  // Add additional widget settings from the property's widget type module.
  $additions = module_invoke($widget_type['module'], 'eck_property_widget_settings_form', $entity_type, $bundle, $property_name, $bundle_property_config);
  if (is_array($additions)) {
    $form['widget'] = array(
      '#tree' => TRUE,
      '#type' => 'fieldset',
      '#title' => t('%type widget settings', array('%type' => $widget_type['label'])),
    );
    $form['widget']['settings'] = $additions;
  }
  
  // Add handling for default value if not provided by any other module.
  if (empty($bundle_property_config['default_value_function'])) {
    $function = $widget_type['module'] . '_eck_property_widget_form';
    if (function_exists($function)) {
      $form['default_value_widget'] = array(
        '#type' => 'fieldset',
        '#title' => t('Default value'),
        '#collapsible' => FALSE,
        '#tree' => TRUE,
        '#description' => t('The default value for this property field, used when creating new content.'),
        // Stick to an empty 'parents' on this form in order not to breaks widgets
        // that do not use field_widget_[field|instance]() and still access
        // $form_state['field'] directly.
        '#parents' => array(),
      );
      $element = array(
        '#entity' => NULL,
        '#entity_type' => $entity_type,
        '#bundle' => $bundle,
        '#property_name' => $property_name,
        '#widget_type' => $widget_type,
        '#language' => LANGUAGE_NONE,
        '#title' => check_plain($bundle_property_config['label']),
        '#description' => field_filter_xss($bundle_property_config['description']),
        '#required' => (!empty($bundle_property_config['required'])),
      );
      // Populate widgets with default values when creating a new entity.
      if ($element = $function($form, $form_state, $property_name, $bundle_property_config, LANGUAGE_NONE, $bundle_property_config['default_value'], $element)) {
        // Allow modules to alter the property widget form element.
        $context = array(
          'form' => $form,
          'property_name' => $property_name,
          'bundle_property_config' => $bundle->config['extra_fields'][$property_name],
          'langcode' => LANGUAGE_NONE,
          'value' => $bundle_property_config['default_value'],
        );
        drupal_alter(array('eck_property_widget_form', 'eck_property_widget_' . $bundle_property_config['widget']['type'] . '_form'), $element, $form_state, $context);
      }
      $form['default_value_widget'][$property_name] = $element;
    }
  }

  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Continue'),
  );
  return $form;
}

/**
 * Submit callback for editing a property field's settings from a bundle's
 * field management form.
 **/
function eck__manage_extra_field_form_submit($form, &$form_state) {
  // Load entity_type and bundle in case they have changed since the form was first built.
  $entity_type = entity_type_load($form['#entity_type']->name);
  $bundle = bundle_load($entity_type->name, $form['#bundle']->name);
  // Update the property field settings.
  if (isset($bundle->config['extra_fields'][$form['#property_name']]['form'])) {
    $property_settings = $bundle->config['extra_fields'][$form['#property_name']];
    $widget_info = eck_property_info_widget_types($property_settings['form']['widget']['type']);
    $property_settings['form']['required'] = !(empty($form_state['values']['required']));
    $property_settings['form']['label'] = $form_state['values']['label'];
    $property_settings['form']['description'] = $form_state['values']['description'];

    // Update the widget settings if there are any
    if (isset($form['widget']['settings'])) {
      $property_settings['form']['widget']['settings'] = $form_state['values']['widget']['settings'];
    }

    // Handle the default value.
    if (isset($form['default_value_widget'])) {
      $property_settings['form']['default_value'] = eck_property_widget_extract_value($entity_type, $bundle, $form['#property_name'], $widget_info, $form, $form_state);
    }

    // Now update the bundle config.
    $config = $bundle->config;
    $config['extra_fields'][$form['#property_name']] = $property_settings;
    $bundle->config = $config;
    // Save the bundle.
    $bundle->save();
    Bundle::loadAll(NULL, TRUE);
  } else {
    // The extra field form setting was not found in the config so it must no longer
    // be managed. Don't do anything except notify the user.
    drupal_set_message(t('Could not edit the property field settings. The property %name on the %bundle bundle is no selected for management.', array('%name' => $form['#property_name'], '%bundle' => $bundle->label)), 'error');
  }
  $form_state['redirect'] = 'admin/structure/entity-type/' . $entity_type->name . '/' . $bundle->name . '/fields';
}

/**
 * Helper function for extracting the value for a property from its widget form.
 * 
 * If one is available the 'value callback' function for the widget is called to
 * processe the values returned.
 * 
 * @param $entity_type
 *   The ECK entity_type object.
 * @param $bundle
 *   The ECK bundle object.
 * @param $property_name
 *   The machine name of the property for which the value should be returned.
 * @param $widget
 *   The widget info array as returned by eck_property_info_widget_types(TYPE).
 * @param $form
 *   The submitted form array.
 * @param $form_state
 *   The array containing the current state information for the form.
 * 
 * @return
 *   The value that should be saved for this property.
 */
function eck_property_widget_extract_value($entity_type, $bundle, $property_name, $widget, $form, &$form_state) {
  $value = NULL;
  // If the widget has a callback to extract the value then use it.
  if (!empty($widget['value callback']) && function_exists($widget['value callback'])) {
    $value = $widget['value callback']($entity_type, $bundle, $property_name, $widget, $form, $form_state);
  } else {
    // By default just take the value matching the property name.
    if (isset($form_state['values'][$property_name])) {
      $value = $form_state['values'][$property_name];
    }
  }
  return $value;
}

/**
 * Form for changing the widget type for a given entity property.
 * 
 * @param $form
 *   The property widget form.
 * @param $form_state
 *   The current state of the form.
 * @param $entity_type
 *   The machine name of the entity type.
 * @param $bundle_type
 *   The machine name of the bundle type.
 * @param $property_name
 *   The machine name of the entity property being included as an extra field.
 * 
 * @return
 *   An array comprising the form for changing the property's widget.
 */
function eck__extra_field_widget_form($form, &$form_state, $entity_type_name, $bundle_type, $property_name) {
  $entity_type = entity_type_load($entity_type_name);
  $bundle = bundle_load($entity_type_name, $bundle_type);
  drupal_set_title($bundle->config['extra_fields'][$property_name]['form']['label']);

  $default_widget = (!empty($bundle->config['extra_fields'][$property_name]['form']['widget']['type'])) ? $bundle->config['extra_fields'][$property_name]['form']['widget']['type'] : '';
  
  $form = array (
    '#entity_type' => $entity_type,
    '#bundle' => $bundle,
    '#property_name' => $property_name,
    '#default_widget' => $default_widget,
  );
  $form['widget'] = array(
    '#type' => 'fieldset', 
    '#title' => t('Change Widget'), 
    '#weight' => 5, 
    '#collapsible' => FALSE, 
  );

  $form['widget']['widget_type'] = array(
    '#type' => 'select',
    '#title' => t('Widget for managed property'),
    '#options' => eck_property_widget_type_options($entity_type->properties[$property_name]['type']),
    '#empty_option' => t('- Select a widget -'),
    '#description' => t('The type of form element you would like to present to the user when managing this property for the %bundle type.', array('%type' => $bundle->label)),
    '#default_value' => $default_widget,
  );
  
  $form['actions'] = array('#type' => 'actions');
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Continue'),
  );
  return $form;
}

/**
 * Validation callback to change the widget for a property field from a bundle's
 * field management form.
 **/
function eck__extra_field_widget_form_validate($form, &$form_state) {
  // Missing widget type.
  if (empty($form_state['values']['widget_type'])) {
    form_set_error('widget_type', t('You need to select a widget.'));
  }
  // Wrong widget type.
  else {
    $entity_type = entity_type_load($form['#bundle']->name);
    $widget_types = eck_property_widget_type_options($entity_type->properties[$form['#property_name']]['type']);
    if (!isset($widget_types[$form_state['values']['widget_type']])) {
      form_set_error('widget_type', t('Invalid widget.'));
    }
  }
}

/**
 * Submit callback to change the widget for a property field from a bundle's
 * field management form.
 **/
function eck__extra_field_widget_form_submit($form, &$form_state) {
  // Load entity_type and bundle in case they have changed since the form was first built.
  $entity_type = entity_type_load($form['#entity_type']->name);
  $bundle = bundle_load($entity_type->name, $form['#bundle']->name);
  // Update the widget settings if it has changed.
  if ($form['#default_widget'] != $form_state['values']['widget_type']) {
    // Grab the config settings for this bundle so we can alter them.
    $config = (!empty($bundle->config)) ? $bundle->config : array();
    if (isset($config['extra_fields'][$form['#property_name']]['form'])) {
      $widget_info = eck_property_info_widget_types($form_state['values']['widget_type']);
      // Changing the widget so make sure to start with the default widget settings.
      $widget = array(
        'type' => $form_state['values']['widget_type'],
        'settings' => $widget_info['default settings'],
      );
      $config['extra_fields'][$form['#property_name']]['form']['widget'] = $widget;
      $bundle->config = $config;
      // Save the bundle.
      $bundle->save();
      Bundle::loadAll(NULL, TRUE);
    } else {
      // The extra field form setting was not found in the config so it must no longer
      // be managed. Don't do anything except notify the user.
      drupal_set_message(t('Could not change the property widget. The property %name on the %bundle bundle is no selected for management.', array('%name' => $form['#property_name'], '%bundle' => $bundle->label)), 'error');
    }
  }
  $form_state['redirect'] = 'admin/structure/entity-type/' . $entity_type->name . '/' . $bundle->name . '/fields';
}

/**
 * Form for removing the property from an entity's list of extra fields.
 * 
 * @param $form
 *   The property widget form.
 * @param $form_state
 *   The current state of the form.
 * @param $entity_type
 *   The machine name of the entity type.
 * @param $bundle_type
 *   The machine name of the bundle type.
 * @param $property_name
 *   The machine name of the entity property being included as an extra field.
 * 
 * @return
 *   An array comprising the form for changing the property's widget.
 */
function eck__remove_extra_field_form($form, &$form_state, $entity_type, $bundle_type, $property_name) {
  $bundle = bundle_load($entity_type, $bundle_type);

  $form = array (
    '#entity_type' => $entity_type,
    '#bundle' => $bundle,
    '#property_name' => $property_name,
  );
  return confirm_form($form, 
    t('Are you sure you want to remove %title as a property field for the %bundle bundle?', array('%title' => $property_name, '%bundle' => $bundle->label)),
    'admin/structure/entity-type/' . $entity_type . '/' . $bundle_type . '/properties/' . $property_name,
    t('This will remove the property as an extra field on the %bundle bundle\'s field management page, but it does not delete the property from the entity type. This cannot be undone.', array('%bundle' => $bundle->label)),
    t('Remove'),
    t('Cancel')
  );
}

/**
 * Callback to remove a property field from a bundle's field management form.
 * The property will no longer show on the entity's creation and edit forms.
 **/
function eck__remove_extra_field_form_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    $property_name = $form['#property_name'];
    $entity_type = $form['#entity_type'];
    $bundle = bundle_load($entity_type, $form['#bundle']->name);
    if (!empty($bundle->config['extra_fields'][$property_name]['form'])) {
      $config = $bundle->config;
      // Unset the extra field form settings for this property to no longer manage it.
      unset($config['extra_fields'][$property_name]['form']);
      $bundle->config = $config;
      // Save the bundle.
      $bundle->save();
      Bundle::loadAll(NULL, TRUE);
    }
  }
  $form_state['redirect'] = 'admin/structure/entity-type/' . $entity_type . '/' . $bundle->name . '/fields';
}