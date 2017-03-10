# -*- coding: utf-8 -*-
require 'spec_helper'
require "rocksdb"

describe RocksDB do
  before do
    @rocksdb = RocksDB::DB.new "/tmp/file"
  end

  it 'should get data' do
    @rocksdb.put("test:read", "1")
    expect(@rocksdb.get("test:read")).to eq "1"
  end

  it 'should put data' do 
    expect(@rocksdb.put("test:put", "2")).to be true
    expect(@rocksdb.get("test:put")).to eq "2"
  end

  it 'should delete data' do 
    @rocksdb.put("test:delete", "3")
    expect(@rocksdb.get("test:delete")).to eq "3"

    expect(@rocksdb.delete("test:delete")).to be true
    expect(@rocksdb.get("test:delete")).to be_nil
  end

  it 'should get multi data' do
    @rocksdb.put("test:multi1", "a")
    @rocksdb.put("test:multi2", "b")
    @rocksdb.put("test:multi3", "c")

    expect(@rocksdb.multi_get(["test:multi1", "test:multi2", "test:multi3"])).to eq ["a", "b", "c"]
  end

  it 'should put data atomic update' do
    @rocksdb.put("test:batch1", "a")
    @rocksdb.delete("test:batch2")

    expect(@rocksdb.get("test:batch1")).to eq "a"
    expect(@rocksdb.get("test:batch")).to be_nil

    batch = RocksDB::Batch.new
    batch.delete("test:batch1")
    batch.put("test:batch2", "b")
    @rocksdb.write(batch)

    expect(@rocksdb.get("test:batch1")).to be_nil
    expect(@rocksdb.get("test:batch2")).to eq "b"
  end

  it 'should use multiple db' do
    @rocksdb2 = RocksDB::DB.new "/tmp/file2"
    
    @rocksdb.put("test:multi_db", "1")
    @rocksdb2.put("test:multi_db", "2")
    
    expect(@rocksdb.get("test:multi_db")).to eq "1"
    expect(@rocksdb2.get("test:multi_db")).to eq "2"
    @rocksdb2.close
  end

  it 'should use japanese charactor' do
    @rocksdb.put("test:japanese", "あいうえお")

    expect(@rocksdb.get("test:japanese")).to eq "あいうえお"
    expect(@rocksdb.multi_get(["test:japanese"])).to eq ["あいうえお"]
  end

  it 'should use each' do
    iter = @rocksdb.each 

    array = []
    @rocksdb.each do |value|
      expect(value).not_to be_empty
      array << value
    end

    rev_array = []
    @rocksdb.reverse_each do |value|
      expect(value).not_to be_empty
      rev_array << value
    end
 

    expect(array).to eq rev_array.reverse

    @rocksdb.each_index do |key|
      expect(key).not_to be_empty
    end

    @rocksdb.each_with_index do |key, value|
      expect(key).not_to be_empty
      expect(value).not_to be_empty
    end

    iter.close
  end

  it 'should exists?' do
    @rocksdb.put("test:exists?", "a")
    @rocksdb.delete("test:noexists?")
    expect(@rocksdb.exists?("test:exists?")).to be true
    expect(@rocksdb.exists?("test:noexists?")).to be false

    expect(@rocksdb.includes?("test:exists?")).to be true
    expect(@rocksdb.includes?("test:noexists?")).to be false

  end

  it 'hash' do
    @rocksdb.delete("test:hash")
    expect(@rocksdb["test:hash"]).to be_nil
    @rocksdb["test:hash"] = "a"
    expect(@rocksdb["test:hash"]).to eq "a"

  end

  context 'compact' do
    it 'works with no parameters' do
      expect(@rocksdb.compact).to eq(true)
    end

    it 'works with one parameter' do
      expect(@rocksdb.compact('a')).to eq(true)
    end

    it 'works with two parameters' do
      expect(@rocksdb.compact('a', 'x')).to eq(true)
    end

    it 'works with nil as first parameter' do
      expect(@rocksdb.compact(nil, 'x')).to eq(true)
    end
  end
  
  after do
    @rocksdb.close
  end
end

