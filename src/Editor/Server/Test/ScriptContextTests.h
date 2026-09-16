#pragma once

// Validates the project script context: require() module resolution (the
// project folder first, the bundled folder behind it, dotted and init.lua
// names), the chunk name convention the debugger relies on, the missing
// module error text, the concrete class a component pushes as, and the
// ScriptBehavior class model (class discovery, one instance per script,
// parameter reflection, triggers, message handlers, sequence methods, and
// the error reporting that keeps the state alive).
bool run_script_context_tests();

bool script_module_test_resolves_project_scripts();
bool script_module_test_resolves_bundled_scripts();
bool script_module_test_chunk_name_is_project_relative();
bool script_module_test_missing_module_names_scripts_folder();
bool script_context_test_pushes_concrete_component_class();

bool script_behavior_test_extend_and_construct();
bool script_behavior_test_class_from_return_value();
bool script_behavior_test_class_from_last_created();
bool script_behavior_test_no_class_is_load_error();
bool script_behavior_test_init_error_is_load_error();
bool script_behavior_test_parameter_reflection();
bool script_behavior_test_store_resolution();
bool script_behavior_test_shared_file_per_instance();
bool script_behavior_test_trigger_targeting();
bool script_behavior_test_trigger_error_keeps_state();
bool script_behavior_test_coroutine_error_keeps_state();
bool script_behavior_test_message_handler_order();
bool script_behavior_test_http_trigger_names();
bool script_behavior_test_call_behavior_method();
bool script_behavior_test_component_ref_refresh();
bool script_error_test_parse_location();
bool script_error_test_resolves_project_then_bundled();
