# SIV (Stable Index Vector)

A header-only C++ library providing a vector container with stable IDs for accessing elements, even after insertions and deletions.

## Features

- **Stable IDs**: Objects are accessed via IDs that remain valid regardless of other insertions/deletions
- **Handle System**: Smart handle objects that can detect if their referenced object has been erased
- **Cache-Friendly**: Data stored contiguously in memory for efficient iteration
- **Header-Only**: Single header file, easy to integrate

## Installation

Simply copy `index_vector.hpp` to your project and include it:

```cpp
#include "index_vector.hpp"
```

## Quick Start

### Basic Usage

```cpp
#include "index_vector.hpp"

struct Entity {
    int x, y;
    std::string name;
};

int main() {
    siv::Vector<Entity> entities;
    
    // Add objects - returns a stable ID
    siv::ID player = entities.emplace_back(0, 0, "Player");
    siv::ID enemy = entities.emplace_back(10, 5, "Enemy");
    
    // Access via ID
    entities[player].x = 5;
    
    // Erase objects - other IDs remain valid
    entities.erase(enemy);
    
    // player ID still works!
    std::cout << entities[player].name << std::endl;
}
```

### Using Handles

Handles are smart references that know when their object has been deleted:

```cpp
siv::Vector<Entity> entities;
siv::ID id = entities.emplace_back(0, 0, "Test");

// Create a handle
siv::Handle<Entity> handle = entities.createHandle(id);

// Use like a pointer
handle->x = 10;
(*handle).y = 20;

// Check validity
if (handle.isValid()) {
    // Safe to use
}

// After erasing, handle becomes invalid
entities.erase(id);
if (!handle) {
    std::cout << "Object was deleted!" << std::endl;
}
```

### Iteration

Iterate directly over the contiguous data:

```cpp
// Range-based for loop
for (auto& entity : entities) {
    entity.x += 1;
}

// Access underlying vector
std::vector<Entity>& data = entities.getData();
```

### Conditional Removal

```cpp
entities.remove_if([](const Entity& e) {
    return e.health <= 0;
});
```

## API Reference

### `siv::Vector<T>`

| Method | Description |
|--------|-------------|
| `push_back(obj)` | Copy object, returns ID |
| `emplace_back(args...)` | Construct in-place, returns ID |
| `erase(id)` | Remove object by ID |
| `operator[](id)` | Access object by ID |
| `size()` / `empty()` | Container size queries |
| `createHandle(id)` | Create a validity-tracking handle |
| `isValid(id, validity_id)` | Check if ID is still valid |
| `reserve(n)` | Pre-allocate memory |
| `clear()` | Remove all objects |

### `siv::Handle<T>`

| Method | Description |
|--------|-------------|
| `operator->` / `operator*` | Access underlying object |
| `isValid()` | Check if referenced object still exists |
| `getID()` | Get the associated ID |
| `operator bool()` | Implicit validity check |

## How It Works

- Objects are stored contiguously in a data vector
- An index vector maps stable IDs to current data positions
- On deletion, the last element is swapped into the gap
- Validity IDs detect use-after-erase scenarios

## Requirements

- C++17 or later
- Standard library only

## License

MIT
