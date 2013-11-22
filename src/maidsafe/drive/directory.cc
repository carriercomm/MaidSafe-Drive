/*  Copyright 2011 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

#include "maidsafe/drive/directory.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "maidsafe/drive/file_context.h"
#include "maidsafe/drive/meta_data.h"
#include "maidsafe/drive/utils.h"
#include "maidsafe/drive/proto_structs.pb.h"

namespace fs = boost::filesystem;

namespace maidsafe {

namespace drive {

namespace detail {

namespace {

bool MetaDataHasName(const MetaData& meta_data, const fs::path& name) {
  return GetLowerCase(meta_data.name.string()) == GetLowerCase(name.string());
}

}  // unnamed namespace

Directory::Directory() : parent_id_(), directory_id_(), max_versions_(kMaxVersions), children_(),
                         children_itr_position_(0) {}

Directory::Directory(ParentId parent_id, DirectoryId directory_id)
    : parent_id_(std::move(parent_id)), directory_id_(std::move(directory_id)),
      max_versions_(kMaxVersions), children_(), children_itr_position_(0) {}

Directory::Directory(Directory&& other)
    : parent_id_(std::move(other.parent_id_)), directory_id_(std::move(other.directory_id_)),
      max_versions_(std::move(other.max_versions_)), children_(std::move(other.children_)),
      children_itr_position_(std::move(other.children_itr_position_)) {}

Directory& Directory::operator=(Directory other) {
  swap(*this, other);
  return *this;
}

Directory::Directory(ParentId parent_id, const std::string& serialised_directory)
    : parent_id_(std::move(parent_id)), directory_id_(), max_versions_(kMaxVersions), children_(),
      children_itr_position_(0) {
  protobuf::Directory proto_directory;
  if (!proto_directory.ParseFromString(serialised_directory))
    ThrowError(CommonErrors::parsing_error);

  directory_id_ = Identity(proto_directory.directory_id());
  max_versions_ = MaxVersions(proto_directory.max_versions());

  for (int i(0); i != proto_directory.children_size(); ++i)
    children_.emplace_back(MetaData(proto_directory.children(i)));
  std::sort(std::begin(children_), std::end(children_));
}

std::string Directory::Serialise() const {
  protobuf::Directory proto_directory;
  proto_directory.set_directory_id(directory_id_.string());
  proto_directory.set_max_versions(max_versions_.data);

  for (const auto& child : children_)
    child.meta_data.ToProtobuf(proto_directory.add_children());

  return proto_directory.SerializeAsString();
}

std::vector<FileContext>::iterator Directory::Find(const fs::path& name) {
  return std::find_if(std::begin(children_), std::end(children_),
                      [&name](const FileContext& file_context) {
                           return MetaDataHasName(file_context.meta_data, name); });
}

std::vector<FileContext>::const_iterator Directory::Find(const fs::path& name) const {
  return std::find_if(std::begin(children_), std::end(children_),
                      [&name](const FileContext& file_context) {
                           return MetaDataHasName(file_context.meta_data, name); });
}

bool Directory::HasChild(const fs::path& name) const {
  return std::any_of(std::begin(children_), std::end(children_),
      [&name](const FileContext& file_context) {
          return MetaDataHasName(file_context.meta_data, name); });
}

void Directory::GetChild(const fs::path& name, MetaData& meta_data) const {
  auto itr(Find(name));
  if (itr == std::end(children_))
    ThrowError(CommonErrors::invalid_parameter);
  meta_data = itr->meta_data;
}

bool Directory::GetChildAndIncrementItr(MetaData& meta_data) {
  if (children_itr_position_ != children_.size()) {
    meta_data = children_[children_itr_position_].meta_data;
    ++children_itr_position_;
    return true;
  }
  return false;
}

void Directory::AddChild(const MetaData& child) {
  auto itr(Find(child.name));
  if (itr != std::end(children_))
    ThrowError(CommonErrors::invalid_parameter);
  children_.emplace_back(child);
  SortAndResetChildrenIterator();
}

void Directory::RemoveChild(const MetaData& child) {
  auto itr(Find(child.name));
  if (itr == std::end(children_))
    ThrowError(CommonErrors::invalid_parameter);
  children_.erase(itr);
  SortAndResetChildrenIterator();
}

void Directory::UpdateChild(const MetaData& child) {
  auto itr(Find(child.name));
  if (itr == std::end(children_))
    ThrowError(CommonErrors::invalid_parameter);
  itr->meta_data = child;
  SortAndResetChildrenIterator();
}

bool Directory::empty() const {
  return children_.empty();
}

void Directory::SortAndResetChildrenIterator() {
  std::sort(std::begin(children_), std::end(children_));
  ResetChildrenIterator();
}

bool operator<(const Directory& lhs, const Directory& rhs) {
  return lhs.directory_id() < rhs.directory_id();
}

void swap(Directory& lhs, Directory& rhs) {
  using std::swap;
  swap(lhs.parent_id_, rhs.parent_id_);
  swap(lhs.directory_id_, rhs.directory_id_);
  swap(lhs.max_versions_, rhs.max_versions_);
  swap(lhs.children_, rhs.children_);
  swap(lhs.children_itr_position_, rhs.children_itr_position_);
}

}  // namespace detail

}  // namespace drive

}  // namespace maidsafe