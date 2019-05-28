#include "rocksdb_rb.h"
#include "rocksdb_db_rb.h"
#include "ruby/encoding.h"
#include <iostream>

extern "C" {
#include <ruby.h>

  VALUE rocksdb_db_init(int argc, VALUE* argv, VALUE self) {
    VALUE v_db_file_name;
    VALUE v_options;
    rocksdb_pointer* db_pointer;
    rocksdb::DB* db;
    rocksdb::Options options;
    rocksdb::Status status;
    std::string db_file_name;
    bool readonly;
    
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    rb_scan_args(argc, argv, "11", &v_db_file_name, &v_options);

    Check_Type(v_db_file_name, T_STRING);
    db_file_name = std::string((char*)RSTRING_PTR(v_db_file_name));

    readonly = false;
    if (TYPE(v_options) == T_HASH) {
      VALUE v = rb_hash_aref(v_options, ID2SYM(rb_intern("readonly")));
      if(v == Qtrue){
        readonly = true;
      }
      v = rb_hash_aref(v_options, ID2SYM(rb_intern("prepare_for_bulk_load")));
      if(v == true){
        options.PrepareForBulkLoad();
      }
      set_opt(&options, &v_options);
    }
    //std::cout << options.max_bytes_for_level_base << "\n";
    //std::cout << options.max_grandparent_overlap_factor << "\n";
    //std::cout << options.delete_obsolete_files_period_micros << "\n";

    options.create_if_missing = true;
    if(readonly){
      status = rocksdb::DB::OpenForReadOnly(options, db_file_name, &db);
    }else{
      status = rocksdb::DB::Open(options, db_file_name, &db);
    }

    db_pointer->db = db;
    db_pointer->readonly = readonly;
    
    return status.ok() ? Qtrue : Qfalse;
  }
  VALUE rocksdb_db_init2(int argc, VALUE* argv, VALUE self) {
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    db_pointer->db = NULL;
    db_pointer->readonly = true;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    return Qtrue;
  }

  void set_opt_unit_val(uint64_t* opt, char* name, VALUE *v_options){

    VALUE v2 = rb_hash_aref(*v_options, ID2SYM(rb_intern(name)));
    if(RB_TYPE_P(v2, T_FIXNUM)){
      *opt = NUM2INT(v2);
    }
  }
  void set_opt_int_val(int* opt, char* name, VALUE *v_options){
    VALUE v2 = rb_hash_aref(*v_options, ID2SYM(rb_intern(name)));
    if(RB_TYPE_P(v2, T_FIXNUM)){
      *opt = NUM2INT(v2);
    }
  }

  void set_opt(rocksdb::Options* options, VALUE *v_options){
    set_opt_unit_val(&options->max_bytes_for_level_base, (char *) "max_bytes_for_level_base", v_options);
    set_opt_unit_val(&options->delete_obsolete_files_period_micros, (char *) "delete_obsolete_files_period_micros", v_options);
    set_opt_int_val(&options->max_open_files, (char *) "max_open_files", v_options);
  }

  VALUE db_alloc(VALUE klass){
    rocksdb_pointer* db_pointer = ALLOC(rocksdb_pointer);
    return Data_Wrap_Struct(klass, 0, db_free, db_pointer);
  }

  VALUE rocksdb_db_put(VALUE self, VALUE v_key, VALUE v_value) {
    Check_Type(v_key, T_STRING);
    Check_Type(v_value, T_STRING);

    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    
    std::string key = std::string((char*)RSTRING_PTR(v_key), RSTRING_LEN(v_key));
    std::string value = std::string((char*)RSTRING_PTR(v_value), RSTRING_LEN(v_value));

    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    if (db_pointer->readonly) {
      rb_raise(rb_eRuntimeError, "readonly");
    }
    rocksdb::Status status = db_pointer->db->Put(rocksdb::WriteOptions(), key, value);
    
    return status.ok() ? Qtrue : Qfalse;
  }

  VALUE rocksdb_db_write(VALUE self, VALUE v_write){
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    rocksdb::WriteBatch *batch;
    Data_Get_Struct(v_write, rocksdb::WriteBatch, batch);

    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    if (db_pointer->readonly) {
      rb_raise(rb_eRuntimeError, "readonly");
    }
    rocksdb::Status status = db_pointer->db->Write(rocksdb::WriteOptions(), batch);
    return status.ok() ? Qtrue : Qfalse;
  }

  VALUE rocksdb_db_property(VALUE self, VALUE v_key){
    Check_Type(v_key, T_STRING);

    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    std::string key = std::string((char*)RSTRING_PTR(v_key), RSTRING_LEN(v_key));
    std::string value;
    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }

    bool status = db_pointer->db->GetProperty(key, &value);
    if(!status) return Qnil;
    return rb_enc_str_new(value.data(), value.size(), rb_utf8_encoding());
  }

  VALUE rocksdb_db_get(VALUE self, VALUE v_key){
    Check_Type(v_key, T_STRING);

    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    std::string key = std::string((char*)RSTRING_PTR(v_key), RSTRING_LEN(v_key));
    std::string value;
    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    rocksdb::Status status = db_pointer->db->Get(rocksdb::ReadOptions(), key, &value);    

    return (status.IsNotFound()) ? Qnil : rb_enc_str_new(value.data(), value.size(), rb_utf8_encoding());

  }


  VALUE rocksdb_db_multi_get(VALUE self, VALUE v_array){
    Check_Type(v_array, T_ARRAY);

    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    long i;
    long length = RARRAY_LEN(v_array);
    std::vector<std::string> values(length);
    std::vector<rocksdb::Slice> keys(length);
    std::vector<rocksdb::Status> status;

    for(i=0; i < length; i++){
      VALUE op = rb_ary_entry(v_array, i);
      keys[i] = rocksdb::Slice((char*)RSTRING_PTR(op), RSTRING_LEN(op));
    }

    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    status = db_pointer->db->MultiGet(rocksdb::ReadOptions(),keys,&values);
    for(i=0; i < length; i++){
      rb_ary_store(v_array, i, rb_enc_str_new(values[i].data(), values[i].size(), rb_utf8_encoding()));
    }
    return v_array;
  }
  
  VALUE rocksdb_db_delete(VALUE self, VALUE v_key){
    Check_Type(v_key, T_STRING);
    
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    std::string key = std::string((char*)RSTRING_PTR(v_key), RSTRING_LEN(v_key));
    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    if (db_pointer->readonly) {
      rb_raise(rb_eRuntimeError, "readonly");
    }
    rocksdb::Status status = db_pointer->db->Delete(rocksdb::WriteOptions(), key);
    
    return status.ok() ? Qtrue : Qfalse;
  }

  VALUE rocksdb_db_exists(VALUE self, VALUE v_key){
    Check_Type(v_key, T_STRING);

    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    std::string key = std::string((char*)RSTRING_PTR(v_key), RSTRING_LEN(v_key));
    std::string value = std::string();
    
    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    return db_pointer->db->KeyMayExist(rocksdb::ReadOptions(), key, &value) ? Qtrue : Qfalse;
  }  

  VALUE rocksdb_db_close(VALUE self){
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    if(db_pointer->db != NULL){
      delete db_pointer->db;
      db_pointer->db = NULL;
    }
    return Qnil;
  }

  void db_free(rocksdb_pointer* db_pointer){
    if(db_pointer->db != NULL){
      delete db_pointer->db;
      db_pointer->db = NULL;
    }
    delete db_pointer;
  }

  VALUE rocksdb_db_new_iterator(VALUE self){
    rocksdb_pointer* db_pointer;
    rocksdb_iterator_pointer* rocksdb_it;

    VALUE klass;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);

    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    rocksdb::Iterator* it = db_pointer->db->NewIterator(rocksdb::ReadOptions());

    klass = rb_class_new_instance(0, NULL, cRocksdb_iterator);

    Data_Get_Struct(klass, rocksdb_iterator_pointer , rocksdb_it);
    rocksdb_it->it = it;
    return klass;
  }


  VALUE rocksdb_db_each(VALUE self){
    if(!rb_block_given_p()){
      return rocksdb_db_new_iterator(self);
    }
    
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    rocksdb::Iterator* it = db_pointer->db->NewIterator(rocksdb::ReadOptions());

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      rb_yield(rb_enc_str_new(it->value().data(), it->value().size(), rb_utf8_encoding()));
    }
    
    delete it;
    return self;
  }

  VALUE rocksdb_db_each_index(VALUE self){
    if(!rb_block_given_p()){
      return rocksdb_db_new_iterator(self);
    }
    
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    rocksdb::Iterator* it = db_pointer->db->NewIterator(rocksdb::ReadOptions());

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      rb_yield(rb_enc_str_new(it->key().data(), it->key().size(), rb_utf8_encoding()));
    }
    
    delete it;
    return self;
  }

  VALUE rocksdb_db_each_with_index(VALUE self){
    if(!rb_block_given_p()){
      return rocksdb_db_new_iterator(self);
    }
    
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    rocksdb::Iterator* it = db_pointer->db->NewIterator(rocksdb::ReadOptions());

    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      VALUE a = rb_enc_str_new(it->key().data(), it->key().size(), rb_utf8_encoding());
      VALUE b = rb_enc_str_new(it->value().data(), it->value().size(), rb_utf8_encoding());
      rb_yield_values(2, a, b);
    }
    
    delete it;
    return self;
  }

  

  VALUE rocksdb_db_reverse_each(VALUE self){
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    rocksdb::Iterator* it = db_pointer->db->NewIterator(rocksdb::ReadOptions());

    for (it->SeekToLast(); it->Valid(); it->Prev()) {
      rb_yield(rb_enc_str_new(it->value().data(), it->value().size(), rb_utf8_encoding()));
    }
    
    delete it;
    return self;
  }

  VALUE rocksdb_db_debug(VALUE self){
    return Qnil;
  }
  VALUE rocksdb_db_is_readonly(VALUE self){
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    if (db_pointer->readonly) {
      return Qtrue;
    }
    if (!db_pointer->readonly) {
      return Qfalse;
    }
    return Qnil;
  }
  VALUE rocksdb_db_is_open(VALUE self){
    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    return (db_pointer->db == NULL) ? Qfalse : Qtrue;
  }
  
  VALUE rocksdb_db_compact(int argc, VALUE* argv, VALUE self) {
    VALUE v_from, v_to;
    rocksdb::Slice from, to;

    rb_scan_args(argc, argv, "02", &v_from, &v_to);

    if(!NIL_P(v_from)) {
      Check_Type(v_from, T_STRING);
      from = rocksdb::Slice((char*)RSTRING_PTR(v_from), RSTRING_LEN(v_from));
    }

    if(!NIL_P(v_to)) {
      Check_Type(v_to, T_STRING);
      to = rocksdb::Slice((char*)RSTRING_PTR(v_to), RSTRING_LEN(v_to));
    }

    rocksdb_pointer* db_pointer;
    Data_Get_Struct(self, rocksdb_pointer, db_pointer);
    if (db_pointer->db == NULL) {
      rb_raise(rb_eRuntimeError, "db not open");
    }
    rocksdb::Status status = db_pointer->db->CompactRange(rocksdb::CompactRangeOptions(), &from, &to);
    return status.ok() ? Qtrue : Qfalse;
  }
}
