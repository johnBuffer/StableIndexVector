#pragma once
#include <limits>
#include <vector>
#include <cassert>


namespace siv
{
    /** Alias the differentiate between IDs and index.
     * An ID allows to access the data through the index vector and is associated with the same object until its erased.
     * An index is simply the current position of the object in the data vector and may change with deletions.
     */
    using ID = uint64_t;

    static constexpr ID InvalidID = std::numeric_limits<ID>::max();

    /// Forward declaration
    template<typename TObjectType>
    class Vector;

    /** A standalone struct allowing to access an object without the need to have a reference to the containing Vector.
     *
     * @tparam TObjectType The type of the object
     */
    /** Standalone object to access an object
     *
     * @tparam TObjectType The object's type
     */
    template<typename TObjectType>
    class Handle
    {
    public:
        /// Default constructor
        Handle() = default;
        /// Constructor
        Handle(ID id, ID validity_id, Vector<TObjectType>* vector)
            : m_id{id}
            , m_validity_id{validity_id}
            , m_vector{vector}
        {}

        /// Pointer-like access to the underlying object
        TObjectType* operator->()
        {
            return &(*m_vector)[m_id];
        }

        /// Const pointer-like access to the object
        TObjectType const* operator->() const
        {
            return &(*m_vector)[m_id];
        }

        /// Dereference operator
        TObjectType& operator*()
        {
            return (*m_vector)[m_id];
        }

        /// Dereference constant operator
        TObjectType const& operator*() const
        {
            return (*m_vector)[m_id];
        }

        /// Returns the ID of the associated object
        [[nodiscard]]
        ID getID() const
        {
            return m_id;
        }

        /** Bool operator to test against the validity of the reference
         *
         * @return false if uninitialized or if the object has been erased from the vector, true otherwise
         */
        explicit operator bool() const
        {
            return isValid();
        }

        /** Check if the reference is associated with a vector and has a correct validity ID
         *
         * @return false if uninitialized or if the object has been erased from the vector, true otherwise
         */
        [[nodiscard]]
        bool isValid() const
        {
            return m_vector && m_vector->isValid(m_id, m_validity_id);
        }

    private:
        /// The ID of the object.
        ID                   m_id          = 0;
        /// The validity ID of the object at the time of creation. Used to check the validity of the handle.
        ID                   m_validity_id = 0;
        /// A raw pointer to the vector containing the object associated with this handle
        Vector<TObjectType>* m_vector      = nullptr;

        /// Used to perform debug checks
        friend class Vector<TObjectType>;
    };

    /** A vector that provide stable IDs when adding objects.
     * These IDs will still allow to access their associated objects even after inserting of removing other objects.
     * This comes at the cost of a small overhead because of an additional indirection.
     *
     * @tparam TObjectType The type of the objects to be stored in the vector. It has to be movable.
     */
    template<typename TObjectType>
    class Vector
    {
    public:
        Vector() = default;

        /** Copies the provided object at the end of the vector
         *
         * @param object The object to copy
         * @return The ID to retrieve the object
         */
        ID push_back(const TObjectType& object)
        {
            const ID id = getFreeSlot();
            m_data.push_back(object);
            return id;
        }

        /** Constructs an object in place
         *
         * @tparam TArgs Constructor arguments types
         * @param args Constructor arguments
         * @return The ID to retrieve the object
         */
        template<typename... TArgs>
        ID emplace_back(TArgs&&... args)
        {
            const ID id = getFreeSlot();
            m_data.emplace_back(std::forward<TArgs>(args)...);
            return id;
        }

        /** Removes the object from the vector
         *
         * @param id The ID of the object to remove
         */
        void erase(ID id)
        {
            // Fetch relevant info
            const ID data_id      = m_indexes[id];
            const ID last_data_id = m_data.size() - 1;
            const ID last_id      = m_metadata[last_data_id].rid;
            // Update validity ID
            ++m_metadata[data_id].validity_id;
            // Swap the object to delete with the object at the end
            std::swap(m_data[data_id], m_data[last_data_id]);
            std::swap(m_metadata[data_id], m_metadata[last_data_id]);
            std::swap(m_indexes[id], m_indexes[last_id]);
            // Destroy the object
            m_data.pop_back();
        }

        /** Removes the object from the vector
         *
         * @param idx The index in the data vector of the object to remove
         */
        void eraseViaData(uint32_t idx)
        {
            erase(m_metadata[idx].rid);
        }

        /** Removes the object referenced by the handle from the vector
         *
         * @param handle The handle referencing the object to remove
         */
        void erase(const Handle<TObjectType>& handle)
        {
            // Ensure the handle is from this vector
            assert(handle.m_vector == this);
            // Ensure the object hasn't already been erased
            assert(handle.isValid());
            erase(handle.getID());
        }

        /** Return the index in the data vector of the object referenced by the provided ID
         *
         * @param id The ID to find the data index of
         * @return The index in the data vector associated with the ID
         */
        [[nodiscard]]
        uint64_t getDataIndex(ID id) const
        {
            return m_indexes[id];
        }

        /** Access the object reference by the provided ID
         *
         * @param id The object's ID
         * @return A reference to the object
         */
        TObjectType& operator[](ID id)
        {
            return m_data[m_indexes[id]];
        }

        /** Access the object reference by the provided ID
         *
         * @param id The object's ID
         * @return A constant reference to the object
         */
         TObjectType const& operator[](ID id) const
        {
            return m_data[m_indexes[id]];
        }

        /// Returns the number of objects in the vector
        [[nodiscard]]
        size_t size() const
        {
            return m_data.size();
        }

        /// Tells if the vector is currently empty
        [[nodiscard]]
        bool empty() const
        {
            return m_data.empty();
        }

        /// Returns the vector's capacity (i.e. the number of allocated slots in the vector)
        [[nodiscard]]
        size_t capacity() const
        {
            return m_data.capacity();
        }

        /** Creates a handle pointing to the provided ID
         *
         * @param id The ID of the object
         * @return A handle to the object
         */
        Handle<TObjectType> createHandle(ID id)
        {
            /* Ensure the object is valid. If the data index is greater than the current size
             * it means that it has been swapped and removed. */
            assert(getDataIndex(id) < size());
            return {id, m_metadata[m_indexes[id]].validity_id, this};
        }

        /** Creates a handle to an object using its position in the data vector
         *
         * @param idx The index of the object in the data vector
         * @return A handle to the object
         */
        Handle<TObjectType> createHandleFromData(uint64_t idx)
        {
            /* Ensure the object is valid. If the data index is greater than the current size
             * it means that it has been swapped and removed. */
            assert(idx < size());
            return {m_metadata[idx].rid, m_metadata[idx].validity_id, this};
        }

        /** Checks if the provided object is still valid considering its last known validity ID
         *
         * @param id The ID of the object
         * @param validity_id The last known validity ID
         * @return True if the last known validity ID is equal to the current one
         */
        [[nodiscard]]
        bool isValid(ID id, ID validity_id) const
        {
            return validity_id == m_metadata[m_indexes[id]].validity_id;
        }

        /// Begin iterator of the data vector
        typename std::vector<TObjectType>::iterator begin() noexcept
        {
            return m_data.begin();
        }

        /// End iterator of the data vector
        typename std::vector<TObjectType>::iterator end() noexcept
        {
            return m_data.end();
        }

        /// Const begin iterator of the data vector
        typename std::vector<TObjectType>::const_iterator begin() const noexcept
        {
            return m_data.begin();
        }

        /// Const end iterator of the data vector
        typename std::vector<TObjectType>::const_iterator end() const noexcept
        {
            return m_data.end();
        }

        /** Removes all objects that match the provided predicate
         *
         * @tparam TCallback The callback's type, any callable should be fine
         * @param callback The predicate used to check an object has to be removed
         */
        template<typename TCallback>
        void remove_if(TCallback&& predicate)
        {
            for (uint32_t i{0}; i < m_data.size();) {
                if (predicate(m_data[i])) {
                    eraseViaData(i);
                } else {
                    ++i;
                }
            }
        }

        /** Pre allocates @p size slots in the vector
         *
         * @param size The number of slots to allocate in the vector
         */
        void reserve(size_t size)
        {
            m_data.reserve(size);
            m_metadata.reserve(size);
            m_indexes.reserve(size);
        }

        /// Return the validity ID associated with the provided ID
        [[nodiscard]]
        ID getValidityID(ID id) const
        {
            return m_metadata[m_indexes[id]].validity_id;
        }

        /// Returns a raw pointer to the first element of the data vector
        TObjectType* data()
        {
            return m_data.data();
        }

        /// Returns a reference to the data vector
        std::vector<TObjectType>& getData()
        {
            return m_data;
        }

        /// Returns a constant reference to the data vector
        const std::vector<TObjectType>& getData() const
        {
            return m_data;
        }

        /// Returns the ID that would be used if an object was added
        [[nodiscard]]
        ID getNextID() const
        {
            // This means that we have available slots
            if (m_metadata.size() > m_data.size()) {
                return m_metadata[m_data.size()].rid;
            }
            return m_data.size();
        }

        /// Erase all objects and invalidates all slots
        void clear()
        {
            // Remove all data
            m_data.clear();

            for (auto& m : m_metadata) {
                // Invalidate all slots
                ++m.validity_id;
            }
        }

        [[nodiscard]]
        bool isValidID(siv::ID id) const
        {
            return id < m_indexes.size();
        }

    private:
        /** Creates a new slot in the vector
         *
         * @note If a slot is available it will be reused, if not a new one will be created.
         *
         * @return The ID of the newly created slot.
         */
        ID getFreeSlot()
        {
            const ID id = getFreeID();
            m_indexes[id] = m_data.size();
            return id;
        }

        /** Gets a ID to a free slot.
         *
         * @note If an ID is available it will be reused, if not a new one will be created.
         *
         * @return An ID of a free slot.
         */
        ID getFreeID()
        {
            // This means that we have available slots
            if (m_metadata.size() > m_data.size()) {
                // Update the validity ID
                ++m_metadata[m_data.size()].validity_id;
                return m_metadata[m_data.size()].rid;
            }
            // A new slot has to be created
            const ID new_id = m_data.size();
            m_metadata.push_back({new_id, 0});
            m_indexes.push_back(new_id);
            return new_id;
        }

    private:
        /// The struct holding additional information about an object
        struct Metadata
        {
            /// The reverse ID, allowing to retrieve the ID of the object from the data vector.
            ID rid         = 0;
            /// An identifier that is changed when the object is erased, used to ensure a handle is still valid.
            ID validity_id = 0;
        };

        /// The vector holding the actual objects.
        std::vector<TObjectType> m_data;
        /// The vector holding the associated metadata. It is accessed using the same index as for the data vector.
        std::vector<Metadata>    m_metadata;
        /// The vector that stores the data index for each ID.
        std::vector<ID>          m_indexes;
    };
}
