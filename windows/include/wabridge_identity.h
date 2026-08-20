#pragma once

#include <filesystem>
#include <string>

namespace wabridge::identity {

struct Material {
    std::string certificate_pem;
    std::string private_key_pem;
    std::string fingerprint;
};

class Store final {
public:
    explicit Store(std::filesystem::path directory = {});
    Material load_or_create();

private:
    std::filesystem::path directory_;
};

} // namespace wabridge::identity
