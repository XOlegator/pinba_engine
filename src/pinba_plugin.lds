/* Export MySQL plugin descriptor symbols so the server can find them. */
PINBA_1.0 {
  global:
    _mysql_plugin_interface_version_;
    _mysql_sizeof_struct_st_plugin_;
    _mysql_plugin_declarations_;
  local:
    *;
};
