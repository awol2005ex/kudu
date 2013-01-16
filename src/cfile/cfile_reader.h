// Copyright (c) 2012, Cloudera, inc.

#ifndef KUDU_CFILE_CFILE_READER_H
#define KUDU_CFILE_CFILE_READER_H

#include <boost/noncopyable.hpp>
#include <boost/shared_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <tr1/memory>
#include <string>

#include "common/columnblock.h"
#include "common/types.h"
#include "cfile/index_btree.h"
#include "gutil/port.h"
#include "util/memory/arena.h"
#include "util/status.h"

namespace kudu {

class RandomAccessFile;

namespace cfile {

class CFileHeaderPB;
class CFileFooterPB;

using std::string;
using boost::shared_array;
using boost::scoped_ptr;
using std::tr1::shared_ptr;


struct ReaderOptions {
};

class CFileIterator;
class BlockPointer;
class BlockDecoder;


// Wrapper for a block of data read from a CFile.
// This reference-counts the underlying data, so it can
// be freely copied, and will not be collected until all copies
// have been destructed.
class BlockData {
public:
  BlockData() {}

  BlockData(const Slice &data,
            shared_array<char> data_for_free) :
    data_(data),
    data_for_free_(data_for_free) {
  }

  BlockData(const BlockData &other) :
    data_(other.data_),
    data_for_free_(other.data_for_free_) {
  }

  const Slice &slice() const {
    return data_;
  }

private:
  Slice data_;
  shared_array<char> data_for_free_;
};

class CFileReader : boost::noncopyable {
public:
  CFileReader(const ReaderOptions &options,
              const shared_ptr<RandomAccessFile> &file,
              uint64_t file_size) :
    options_(options),
    file_(file),
    file_size_(file_size),
    state_(kUninitialized) {
  }

  Status Init();

  Status NewIterator(CFileIterator **iter) const;
  Status NewIterator(scoped_ptr<CFileIterator> *iter) const {
    CFileIterator *iter_ptr;
    RETURN_NOT_OK(NewIterator(&iter_ptr));
    (*iter).reset(iter_ptr);
    return Status::OK();
  }

  // TODO: make this private? should only be used
  // by the iterator and index tree readers, I think.
  Status ReadBlock(const BlockPointer &ptr,
                   BlockData *ret) const;

  // Return the number of rows in this cfile.
  // This is assumed to be reasonably fast (i.e does not scan
  // the data)
  Status CountRows(size_t *count) const;

  DataType data_type() const {
    CHECK_EQ(state_, kInitialized);
    return footer_->data_type();
  }

  const TypeInfo *type_info() const {
    DCHECK_EQ(state_, kInitialized);
    return type_info_;
  }


  // Advanced access to the cfile. This is used by the
  // delta reader code. TODO: think about reorganizing this:
  // delta files can probably be done more cleanly.

  // Return true if there is a position-based index on this file.
  bool has_posidx() const { return footer_->has_posidx_info(); }
  BlockPointer posidx_root() const {
    DCHECK(has_posidx());
    return BlockPointer(footer_->posidx_info().root_block());
  }

  // Return true if there is a value-based index on this file.
  bool has_validx() const { return footer_->has_validx_info(); }
  BlockPointer validx_root() const {
    DCHECK(has_validx());
    return BlockPointer(footer_->validx_info().root_block());
  }

  

private:
  friend class CFileIterator;

  // Create a BlockDecoder for the data in this file.
  // Sets *bd to the newly created decoder, if successful.
  // Otherwise returns a non-OK Status.
  Status CreateBlockDecoder(BlockDecoder **bd, const Slice &slice) const;

  Status ReadMagicAndLength(uint64_t offset, uint32_t *len);
  Status ReadAndParseHeader();
  Status ReadAndParseFooter();

#ifdef __clang__
  __attribute__((__unused__))
#endif
  const ReaderOptions options_;
  const shared_ptr<RandomAccessFile> file_;
  const uint64_t file_size_;

  enum State {
    kUninitialized,
    kInitialized
  };
  State state_;

  scoped_ptr<CFileHeaderPB> header_;
  scoped_ptr<CFileFooterPB> footer_;

  const TypeInfo *type_info_;
};


class CFileIterator : boost::noncopyable {
public:
  CFileIterator(const CFileReader *reader,
                const BlockPointer *posidx_root,
                const BlockPointer *validx_root);

  // Seek to the given ordinal entry in the file.
  // Entry 0 is the first entry written to the file.
  // If provided seek point is past the end of the file,
  // then returns a NotFound Status.
  // TODO: do we ever want to be able to seek to the end of the file?
  Status SeekToOrdinal(uint32_t ord_idx);

  // Seek to the given key, or to the entry directly following
  // it. If the largest key in the file is still less than
  // the given key, then returns a NotFound Status.
  //
  // Sets *exact_match to indicate whether the seek found the exact
  // key requested.
  //
  // If this iterator was constructed without no value index,
  // then this will return a NotSupported status.
  Status SeekAtOrAfter(const void *key,
                       bool *exact_match);

  // Get the ordinal index that the iterator is currently
  // pointed to.
  uint32_t GetCurrentOrdinal() const;

  // Copy up to 'n' values into 'out'.
  // The 'out' buffer must have enough space already allocated for
  // n items.
  // Any indirected values (eg strings) are copied into the dst block's
  // arena.
  // The number of values actually read is written back into 'n'.
  Status CopyNextValues(size_t *n, ColumnBlock *dst);

  bool HasNext();

private:
  // Read the data block currently pointed to by idx_iter_
  // into the dblk_data_ and dblk_ fields.
  //
  // If this returns an error, then the fields
  // have undefined values.
  Status ReadCurrentDataBlock(const IndexTreeIterator &idx_iter);

  const CFileReader *reader_;

  scoped_ptr<IndexTreeIterator> posidx_iter_;
  scoped_ptr<IndexTreeIterator> validx_iter_;

  IndexTreeIterator *seeked_;

  BlockData dblk_data_;
  scoped_ptr<BlockDecoder> dblk_;
};


} // namespace cfile
} // namespace kudu

#endif
