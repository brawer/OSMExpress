#include "osmx/storage.h"
#include "osmx/util.h"

namespace osmx { namespace db {


MDB_env *createEnv(std::string path, bool writable) {
  MDB_env* env;
  CHECK_LMDB(mdb_env_create(&env));

  // the maximum size of any LMDB dataset. 
  // 2TB is a safe number for just OSM data as of 02/2023
  // only affects the size of virtual memory, not real memory.
  mdb_env_set_mapsize(env,2UL * 1024UL * 1024UL * 1024UL * 1024UL);
  mdb_env_set_maxdbs(env,10);
  int flags = 0;
  if (!writable) flags |= MDB_RDONLY;
  CHECK_LMDB(mdb_env_open(env, path.c_str(),MDB_NOSUBDIR | MDB_NORDAHEAD | MDB_NOSYNC | flags, 0664));
  return env;
}

Metadata::Metadata(MDB_txn *txn) : mTxn(txn) {
  CHECK_LMDB(mdb_dbi_open(mTxn, "metadata", MDB_CREATE, &mDbi));
}

void Metadata::put(const std::string &key_str, const std::string &value_str) {
    MDB_val key, data;
    key.mv_size = key_str.size();
    key.mv_data = (void *)key_str.data();
    data.mv_size = value_str.size();
    data.mv_data = (void *)value_str.data();
    CHECK_LMDB(mdb_put(mTxn,mDbi, &key, &data, 0));
}

std::string Metadata::get(const std::string &key_str) {
    MDB_val key, data;
    key.mv_size = key_str.size();
    key.mv_data = (void *)key_str.data();
    auto retval = mdb_get(mTxn,mDbi, &key, &data);
    if (retval == 0) return std::string((const char *)data.mv_data,data.mv_size);
    else return "";
}

Elements::Elements(MDB_txn *txn, const std::string &name) : mTxn(txn) {
  CHECK_LMDB(mdb_dbi_open(txn, name.c_str(), MDB_INTEGERKEY | MDB_CREATE, &mDbi));
}

void Elements::put(uint64_t id, kj::VectorOutputStream &vos, int flags) {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&id;
  data.mv_size = vos.getArray().size();
  data.mv_data = (void *)vos.getArray().begin();
  CHECK_LMDB(mdb_put(mTxn, mDbi, &key, &data, flags));
}

void Elements::del(uint64_t id) {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&id;
  mdb_del(mTxn, mDbi, &key, &data);
}

bool Elements::exists(uint64_t id) {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&id;
  return mdb_get(mTxn,mDbi,&key,&data) == 0;
}

capnp::FlatArrayMessageReader Elements::getReader(uint64_t id) {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&id;
  CHECK_LMDB(mdb_get(mTxn,mDbi,&key,&data));
  auto arr = kj::ArrayPtr<const capnp::word>((const capnp::word *)data.mv_data,data.mv_size);
  return capnp::FlatArrayMessageReader(arr);
}

Locations::Locations(MDB_txn *txn) : mTxn(txn) {
    CHECK_LMDB(mdb_dbi_open(mTxn, "locations", MDB_INTEGERKEY | MDB_CREATE, &mDbi));
}

void Locations::put(uint64_t id, const Location value, int flags) {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&id;

  int32_t buf[3];
  buf[0] = value.coords.x();
  buf[1] = value.coords.y();
  buf[2] = value.version;

  data.mv_size = sizeof(uint32_t) * 3;
  data.mv_data = (void *)&buf;
  CHECK_LMDB(mdb_put(mTxn, mDbi, &key, &data, flags));
}

void Locations::del(uint64_t id) {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&id;
  mdb_del(mTxn,mDbi,&key,&data);
}

Location Locations::get(uint64_t id) const {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&id;
  int retval = mdb_get(mTxn, mDbi, &key, &data);
  if (retval == MDB_NOTFOUND) return Location{};
  CHECK_LMDB(retval);
  int32_t *buf = (int32_t *)data.mv_data;
  return Location{osmium::Location(buf[0],buf[1]),buf[2]};
}

bool Locations::exists(uint64_t id) {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&id;
  int retval = mdb_get(mTxn, mDbi, &key, &data);
  return retval != MDB_NOTFOUND;
}

Index::Index(MDB_txn *txn, const std::string &name) : mTxn(txn) {
  CHECK_LMDB(mdb_dbi_open(txn, name.c_str(), MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED | MDB_INTEGERDUP, &mDbi));
}

void Index::put(uint64_t from, uint64_t osm_id, int flags) {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&from;
  data.mv_size = sizeof(uint64_t);
  data.mv_data = (void *)&osm_id;
  CHECK_LMDB(mdb_put(mTxn,mDbi,&key,&data,flags));
}

void Index::del(uint64_t from, uint64_t osm_id ) {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&from;
  data.mv_size = sizeof(uint64_t);
  data.mv_data = (void *)&osm_id;
  mdb_del(mTxn,mDbi,&key,&data);
}

IndexWriter::IndexWriter(MDB_env *env, const std::string &name) : mEnv(env), mName(name) {
  CHECK_LMDB(mdb_txn_begin(env, NULL, 0, &mTxn));
  CHECK_LMDB(mdb_dbi_open(mTxn, name.c_str(), MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED | MDB_INTEGERDUP, &mDbi));
}

void IndexWriter::put(uint64_t from, uint64_t osm_id, int flags) {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&from;
  data.mv_size = sizeof(uint64_t);
  data.mv_data = (void *)&osm_id;
  CHECK_LMDB(mdb_put(mTxn,mDbi,&key,&data,flags));
  if (mWrites++ == 8000000) {
    CHECK_LMDB(mdb_txn_commit(mTxn));
    CHECK_LMDB(mdb_txn_begin(mEnv, NULL, 0, &mTxn));
    CHECK_LMDB(mdb_dbi_open(mTxn, mName.c_str(), MDB_INTEGERKEY | MDB_CREATE | MDB_DUPSORT | MDB_DUPFIXED | MDB_INTEGERDUP, &mDbi));
    mWrites = 0;
  }
}

void IndexWriter::commit() {
  CHECK_LMDB(mdb_txn_commit(mTxn));
}

void traverseCell(MDB_cursor *cursor,S2CellId cell_id,Roaring64Map &set) {
  S2CellId start = cell_id.child_begin(CELL_INDEX_LEVEL);
  S2CellId end = cell_id.child_end(CELL_INDEX_LEVEL);
  MDB_val key, data;
  key.mv_size = sizeof(S2CellId);
  key.mv_data = (void *)&start;

  // reading past end of db
  if (mdb_cursor_get(cursor,&key,&data,MDB_SET_RANGE) != 0) return;
  while (*((S2CellId *)key.mv_data) < end) {
    int retval_values = mdb_cursor_get(cursor,&key,&data,MDB_GET_MULTIPLE);
    while (0 == retval_values) {
      for (int i = 0; i < data.mv_size/sizeof(uint64_t); i++) {
        uint64_t *d = (uint64_t*)data.mv_data;
        set.add(*(d+i));
      }
      retval_values = mdb_cursor_get(cursor,&key,&data,MDB_NEXT_MULTIPLE);
    }
    // reached end of db
    if (mdb_cursor_get(cursor,&key,&data,MDB_NEXT_NODUP) != 0) return;
  }
}

void traverseReverse(MDB_cursor *cursor,uint64_t from, Roaring64Map &set) {
  MDB_val key, data;
  key.mv_size = sizeof(uint64_t);
  key.mv_data = (void *)&from;

  if (mdb_cursor_get(cursor,&key,&data,MDB_SET) != 0) return;
  int retval_values = mdb_cursor_get(cursor,&key,&data,MDB_GET_MULTIPLE);
  while (0 == retval_values) {
    for (int i = 0; i < data.mv_size/sizeof(uint64_t); i++) {
      uint64_t *d = (uint64_t*)data.mv_data;
      uint64_t to_id = *(d+i);
      set.add(to_id);
    }
    retval_values = mdb_cursor_get(cursor,&key,&data,MDB_NEXT_MULTIPLE);
  }
}

}}

