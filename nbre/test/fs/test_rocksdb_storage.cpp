
#include "fs/nbre_storage.h"
#include "fs/proto/block.pb.h"
#include "fs/proto/ir.pb.h"
#include "fs/rocksdb_storage.h"
#include "fs/util.h"

int main(int argc, char *argv[]) {

  std::string cur_path = neb::fs::cur_dir();
  std::string db_path = neb::fs::join_path(cur_path, "test_data.db");
  neb::fs::rocksdb_storage rs;
  rs.open_database(db_path, neb::fs::storage_open_for_readwrite);
  rs.put("nr", neb::util::number_to_byte<neb::util::bytes>(
                   static_cast<neb::block_height_t>(1)));

  auto f = [](rocksdb::Iterator *it) {
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
      LOG(INFO) << it->key().ToString() << ',' << it->value().ToString();
    }
  };
  rs.show_all(f);

  return 0;
}
