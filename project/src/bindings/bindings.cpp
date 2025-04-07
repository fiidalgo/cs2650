#include <pybind11/pybind11.h>
#include <pybind11/functional.h>
#include <pybind11/stl.h>
#include "naive/lsm_tree.h"

namespace py = pybind11;

PYBIND11_MODULE(lsm_tree_binding, m) {
    m.doc() = "LSM-Tree implementation bindings";
    
    // Create a submodule for the naive implementation
    py::module naive = m.def_submodule("naive", "Naive LSM-Tree implementation");
    
    // Expose GetMetadata
    py::class_<lsm::naive::GetMetadata>(naive, "GetMetadata")
        .def(py::init<>())
        .def_readwrite("sstables_accessed", &lsm::naive::GetMetadata::sstables_accessed)
        .def_readwrite("bytes_read", &lsm::naive::GetMetadata::bytes_read);
    
    // Expose the LSMTree class
    py::class_<lsm::naive::LSMTree>(naive, "LSMTree")
        .def(py::init<std::string, size_t>(),
             py::arg("data_dir"), 
             py::arg("memtable_size_bytes") = 1024 * 1024)
        .def("put", &lsm::naive::LSMTree::put,
             py::arg("key"), py::arg("value"))
        .def("get", [](const lsm::naive::LSMTree& tree, const std::string& key, bool return_metadata) {
            lsm::naive::GetMetadata metadata;
            auto result = tree.get(key, return_metadata ? &metadata : nullptr);
            
            if (return_metadata) {
                py::dict metadata_dict;
                metadata_dict["sstables_accessed"] = metadata.sstables_accessed;
                metadata_dict["bytes_read"] = metadata.bytes_read;
                
                return py::make_tuple(result ? py::cast(*result) : py::none(), metadata_dict);
            } else {
                return py::make_tuple(result ? py::cast(*result) : py::none(), py::none());
            }
        }, py::arg("key"), py::arg("return_metadata") = false)
        .def("range", [](const lsm::naive::LSMTree& tree, const std::string& start_key, const std::string& end_key) {
            py::list results;
            tree.range(start_key, end_key, [&results](const std::string& key, const std::string& value) {
                results.append(py::make_tuple(key, value));
            });
            return results;
        }, py::arg("start_key"), py::arg("end_key"))
        .def("delete", &lsm::naive::LSMTree::remove, py::arg("key"))
        .def("flush", &lsm::naive::LSMTree::flush)
        .def("compact", &lsm::naive::LSMTree::compact)
        .def("close", &lsm::naive::LSMTree::close)
        .def("get_sstable_count", &lsm::naive::LSMTree::getSStableCount)
        .def("get_memtable_size", &lsm::naive::LSMTree::getMemTableSize)
        .def("get_total_size_bytes", &lsm::naive::LSMTree::getTotalSizeBytes)
        .def("get_stats", [](const lsm::naive::LSMTree& tree) {
            std::string stats_json = tree.getStats();
            return py::module::import("json").attr("loads")(stats_json);
        })
        .def("clear", &lsm::naive::LSMTree::clear);
} 