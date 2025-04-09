#ifndef NAIVE_MANIFEST_H
#define NAIVE_MANIFEST_H

#include <string>
#include <vector>

namespace naive
{

    /**
     * Manifest - Tracks metadata about SSTables
     *
     * This is a placeholder implementation that will be expanded later.
     */
    class Manifest
    {
    public:
        /**
         * Constructor - Creates or loads a manifest
         * @param data_dir Directory where the manifest file is stored
         */
        explicit Manifest(const std::string &data_dir);

        /**
         * Destructor
         */
        ~Manifest();

    private:
        // Will be implemented later
    };

} // namespace naive

#endif // NAIVE_MANIFEST_H