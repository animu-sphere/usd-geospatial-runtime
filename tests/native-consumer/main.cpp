// SPDX-License-Identifier: Apache-2.0
#include <usdAssetIo/Diagnostics.h>
#include <cstring>
#include <iostream>

int main() {
    const auto* name = usdasset::StatusCodeName(usdasset::StatusCode::AssetChanged);
    std::cout << name << '\n';
    return std::strcmp(name, "AssetChanged") == 0 ? 0 : 1;
}
