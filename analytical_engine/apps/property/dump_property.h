/** Copyright 2020 Alibaba Group Holding Limited.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef ANALYTICAL_ENGINE_APPS_PROPERTY_DUMP_PROPERTY_H_
#define ANALYTICAL_ENGINE_APPS_PROPERTY_DUMP_PROPERTY_H_
#include <sys/stat.h>
#include <cstdio>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "core/app/property_app_base.h"
#include "core/context/vertex_data_context.h"
#include "grape/grape.h"

namespace gs {
template <typename FRAG_T>
class DumpPropertyContext : public LabeledVertexDataContext<FRAG_T, double> {
  using vid_t = typename FRAG_T::vid_t;
  using oid_t = typename FRAG_T::oid_t;
  using label_id_t = typename FRAG_T::label_id_t;

 public:
  explicit DumpPropertyContext(const FRAG_T& fragment)
      : LabeledVertexDataContext<FRAG_T, double>(fragment, true) {}

  void Init(grape::DefaultMessageManager& messages,
            const std::string& out_prefix_, bool write_eid_) {
    out_prefix = out_prefix_;
    write_eid = write_eid_;
  }

  void Output(std::ostream& os) override {}

  std::string out_prefix;
  bool write_eid{};
  int step{};
};

template <typename FRAG_T>
class DumpProperty
    : public PropertyAppBase<FRAG_T, DumpPropertyContext<FRAG_T>> {
 public:
  INSTALL_DEFAULT_PROPERTY_WORKER(DumpProperty<FRAG_T>,
                                  DumpPropertyContext<FRAG_T>, FRAG_T)
  using vid_t = typename FRAG_T::vid_t;

  using vertex_t = typename FRAG_T::vertex_t;
  using label_id_t = typename FRAG_T::label_id_t;
  using prop_id_t = typename FRAG_T::prop_id_t;

  void PEval(const fragment_t& frag, context_t& ctx,
             message_manager_t& messages) {
    auto& out_prefix = ctx.out_prefix;
    if (frag.fid() == 0) {
      if (access(out_prefix.c_str(), F_OK) != 0) {
        CHECK_EQ(mkdir(out_prefix.c_str(), 0777), 0)
            << "Failed to create " << out_prefix;
      }
    }
    messages.ForceContinue();
  }

  bool Append(const std::vector<std::string>& in_files,
              const std::string& out) {
    std::ofstream out_file(out.c_str(), std::ios_base::out);
    size_t valid_lines = 0;

    for (size_t i = 0; i < in_files.size(); i++) {
      bool skip_first = i != 0;
      auto& in = in_files[i];
      std::ifstream in_file(in.c_str());
      std::string line;
      size_t count = 0;

      while (getline(in_file, line)) {
        count++;
        if (count == 1 && skip_first) {
          continue;
        }
        out_file << line << "\n";
        valid_lines++;
      }

      in_file.close();
      LOG(INFO) << "From " << in << " Append to " << out
                << " Current Line#:" << valid_lines
                << " Line# of This Batch: " << count;
    }
    out_file.close();
    return valid_lines > 1;
  }

  void IncEval(const fragment_t& frag, context_t& ctx,
               message_manager_t& messages) {
    auto& out_prefix = ctx.out_prefix;
    label_id_t v_label_num = frag.vertex_label_num();
    label_id_t e_label_num = frag.edge_label_num();
    auto& schema = frag.schema();
    auto fid = frag.fid();

    if (ctx.step == frag.fnum()) {
      if (fid == 0) {
        for (label_id_t vlabel = 0; vlabel < v_label_num; ++vlabel) {
          auto vlabel_name = schema.GetVertexLabelName(vlabel);
          auto merged_vpath = out_prefix + "/vertex_" + vlabel_name + ".csv";
          std::vector<std::string> vpaths;

          for (fid_t fid = 0; fid < frag.fnum(); fid++) {
            auto vpath = out_prefix + "/" + "frag_" + std::to_string(fid) +
                         "_vertex_" + vlabel_name + ".csv";

            vpaths.push_back(vpath);
          }

          bool append_success = Append(vpaths, merged_vpath);

          for (auto& vpath : vpaths) {
            remove(vpath.c_str());
          }

          if (!append_success) {
            remove(merged_vpath.c_str());
          }
        }

        for (label_id_t src_vlabel = 0; src_vlabel < v_label_num;
             ++src_vlabel) {
          auto src_vlabel_name = schema.GetVertexLabelName(src_vlabel);

          for (label_id_t dst_vlabel = 0; dst_vlabel < v_label_num;
               ++dst_vlabel) {
            auto dst_vlabel_name = schema.GetVertexLabelName(dst_vlabel);
            for (label_id_t elabel = 0; elabel < e_label_num; ++elabel) {
              auto elabel_name = schema.GetEdgeLabelName(elabel);
              auto merged_epath = out_prefix + "/edge_" + src_vlabel_name +
                                  "_" + dst_vlabel_name + "_" + elabel_name +
                                  ".csv";

              std::vector<std::string> epaths;

              for (fid_t fid = 0; fid < frag.fnum(); fid++) {
                auto epath = out_prefix + "/" + "frag_" + std::to_string(fid) +
                             "_edge_" + src_vlabel_name + "_" +
                             dst_vlabel_name + "_" + elabel_name + ".csv";
                epaths.push_back(epath);
              }

              bool append_success = Append(epaths, merged_epath);

              for (auto& epath : epaths) {
                remove(epath.c_str());
              }

              // remove empty relation
              if (!append_success) {
                remove(merged_epath.c_str());
              }
            }
          }
        }
      }
    } else {
      if (ctx.step == fid) {
        auto begin = grape::GetCurrentTime();
        for (label_id_t vlabel = 0; vlabel < v_label_num; ++vlabel) {
          auto vlabel_name = schema.GetVertexLabelName(vlabel);
          auto vprop_num = frag.vertex_property_num(vlabel);
          auto iv = frag.InnerVertices(vlabel);

          std::ofstream os;
          os.open(out_prefix + "/" + "frag_" + std::to_string(fid) +
                  "_vertex_" + vlabel_name + ".csv");
          int vprop_id = -1;

          for (prop_id_t vprop = 0; vprop < vprop_num; vprop++) {
            auto vprop_name = schema.GetVertexPropertyName(vlabel, vprop);

            if (vprop_name == "id") {
              CHECK_EQ(vprop_id, -1) << "Found multiple id columns";
              vprop_id = vprop;
            }
          }

          CHECK_NE(vprop_id, -1) << "Failed to find id column";

          os << schema.GetVertexPropertyName(vlabel, vprop_id);
          for (prop_id_t vprop = 0; vprop < vprop_num; vprop++) {
            auto name = schema.GetVertexPropertyName(vlabel, vprop);

            if (vprop != vprop_id && !name.empty()) {
              os << "," << name;
            }
          }
          os << "\n";

          for (auto v : iv) {
            auto id_type = schema.GetVertexPropertyType(vlabel, vprop_id);
            // id column
            if (id_type == arrow::uint64()) {
              os << frag.template GetData<uint64_t>(v, vprop_id);
            } else if (id_type == arrow::uint32()) {
              os << frag.template GetData<uint32_t>(v, vprop_id);
            } else if (id_type == arrow::int64()) {
              os << frag.template GetData<int64_t>(v, vprop_id);
            } else if (id_type == arrow::int32()) {
              os << frag.template GetData<int32_t>(v, vprop_id);
            } else if (id_type == arrow::float32()) {
              os << frag.template GetData<float>(v, vprop_id);
            } else if (id_type == arrow::float64()) {
              os << frag.template GetData<double>(v, vprop_id);
            } else if (id_type == arrow::large_binary() ||
                       id_type == arrow::large_utf8()) {
              os << frag.template GetData<std::string>(v, vprop_id);
            } else {
              CHECK(false);
            }

            for (prop_id_t vprop = 0; vprop < vprop_num; vprop++) {
              auto col_type = schema.GetVertexPropertyType(vlabel, vprop);
              auto name = schema.GetVertexPropertyName(vlabel, vprop);

              if (vprop != vprop_id && !name.empty()) {
                if (col_type == arrow::uint64()) {
                  os << "," << frag.template GetData<uint64_t>(v, vprop);
                } else if (col_type == arrow::uint32()) {
                  os << "," << frag.template GetData<uint32_t>(v, vprop);
                } else if (col_type == arrow::int64()) {
                  os << "," << frag.template GetData<int64_t>(v, vprop);
                } else if (col_type == arrow::int32()) {
                  os << "," << frag.template GetData<int32_t>(v, vprop);
                } else if (col_type == arrow::float32()) {
                  os << "," << frag.template GetData<float>(v, vprop);
                } else if (col_type == arrow::float64()) {
                  os << "," << frag.template GetData<double>(v, vprop);
                } else if (col_type == arrow::large_binary() ||
                           col_type == arrow::large_utf8()) {
                  os << "," << frag.template GetData<std::string>(v, vprop);
                } else {
                  CHECK(false);
                }
              }
            }
            os << "\n";
          }
          os.close();
        }

        std::map<std::tuple<label_id_t, label_id_t, label_id_t>,
                 std::unique_ptr<std::ofstream>>
            fds;

        for (label_id_t src_vlabel = 0; src_vlabel < v_label_num;
             ++src_vlabel) {
          auto src_vlabel_name = schema.GetVertexLabelName(src_vlabel);

          for (label_id_t dst_vlabel = 0; dst_vlabel < v_label_num;
               ++dst_vlabel) {
            auto dst_vlabel_name = schema.GetVertexLabelName(dst_vlabel);
            for (label_id_t elabel = 0; elabel < e_label_num; ++elabel) {
              auto elabel_name = schema.GetEdgeLabelName(elabel);
              auto k = std::make_tuple(src_vlabel, dst_vlabel, elabel);
              auto os = std::make_unique<std::ofstream>();

              os->open(out_prefix + "/" + "frag_" + std::to_string(fid) +
                       "_edge_" + src_vlabel_name + "_" + dst_vlabel_name +
                       "_" + elabel_name + ".csv");
              (*os) << "src,dst";

              for (prop_id_t prop_id = 0;
                   prop_id < frag.edge_property_num(elabel); prop_id++) {
                auto prop_name = schema.GetEdgePropertyName(elabel, prop_id);
                if (prop_name != "eid" || ctx.write_eid) {
                  (*os) << "," << prop_name;
                }
              }
              (*os) << "\n";
              fds.emplace(k, std::move(os));
            }
          }
        }

        for (label_id_t src_vlabel = 0; src_vlabel < v_label_num;
             ++src_vlabel) {
          auto iv = frag.InnerVertices(src_vlabel);

          for (auto u : iv) {
            for (label_id_t elabel = 0; elabel < e_label_num; ++elabel) {
              auto es = frag.GetOutgoingAdjList(u, elabel);
              for (auto& e : es) {
                auto v = e.neighbor();
                auto dst_vlabel = frag.vertex_label(v);
                auto k = std::make_tuple(src_vlabel, dst_vlabel, elabel);
                auto& os = *fds.at(k);

                os << frag.GetId(u) << "," << frag.GetId(v);
                for (prop_id_t prop_id = 0;
                     prop_id < frag.edge_property_num(elabel); prop_id++) {
                  auto eprop_type = schema.GetEdgePropertyType(elabel, prop_id);
                  auto prop_name = schema.GetEdgePropertyName(elabel, prop_id);

                  if (prop_name != "eid" || ctx.write_eid) {
                    if (eprop_type == arrow::uint64()) {
                      os << "," << e.template get_data<uint64_t>(prop_id);
                    } else if (eprop_type == arrow::uint32()) {
                      os << "," << e.template get_data<uint32_t>(prop_id);
                    } else if (eprop_type == arrow::int64()) {
                      os << "," << e.template get_data<int64_t>(prop_id);
                    } else if (eprop_type == arrow::int32()) {
                      os << "," << e.template get_data<int32_t>(prop_id);
                    } else if (eprop_type == arrow::float32()) {
                      os << "," << e.template get_data<float>(prop_id);
                    } else if (eprop_type == arrow::float64()) {
                      os << "," << e.template get_data<double>(prop_id);
                    } else if (eprop_type == arrow::large_binary() ||
                               eprop_type == arrow::large_utf8()) {
                      os << "," << e.template get_data<std::string>(prop_id);
                    } else {
                      CHECK(false);
                    }
                  }
                }
                os << "\n";
              }
            }
          }
        }

        for (auto& kv : fds) {
          kv.second->close();
        }
        LOG(INFO) << "fid: " << fid
                  << " Dump Time: " << grape::GetCurrentTime() - begin;
      }
      messages.ForceContinue();
    }
    ctx.step++;
  }
};
}  // namespace gs
#endif  // ANALYTICAL_ENGINE_APPS_PROPERTY_DUMP_PROPERTY_H_
