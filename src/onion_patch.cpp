    // --- PATCH: Read V3 Onion Address from Disk ---
    boost::filesystem::path pathOnion = GetDataDir() / "onion" / "hostname";
    if (boost::filesystem::exists(pathOnion)) {
        std::ifstream file(pathOnion.string().c_str());
        if (file.good()) {
            std::string onionAddress;
            std::getline(file, onionAddress);
            if (!onionAddress.empty()) {
                UniValue result(UniValue::VOBJ);
                result.push_back(Pair("onion", onionAddress));
                return result; 
            }
        }
    }
    // ----------------------------------------------
