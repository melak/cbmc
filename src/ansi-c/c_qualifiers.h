/*******************************************************************\

Module:

Author: Daniel Kroening, kroening@kroening.com

\*******************************************************************/


#ifndef CPROVER_ANSI_C_C_QUALIFIERS_H
#define CPROVER_ANSI_C_C_QUALIFIERS_H

#include <iosfwd>
#include <memory>
#include <string>

class typet;

class qualifierst
{
protected:
  // Only derived classes can construct
  qualifierst() = default;

public:
  // Copy/move construction/assignment is deleted here and in derived classes
  qualifierst(const qualifierst &) = delete;
  qualifierst(qualifierst &&) = delete;
  qualifierst &operator=(const qualifierst &) = delete;
  qualifierst &operator=(qualifierst &&) = delete;

  // Destruction is virtual
  virtual ~qualifierst() = default;

public:
  virtual std::unique_ptr<qualifierst> clone() const = 0;

  virtual std::size_t count() const = 0;

  virtual void clear() = 0;

  virtual void read(const typet &src) = 0;
  virtual void write(typet &src) const = 0;

  // String conversion
  virtual std::string as_string() const = 0;
  friend std::ostream &operator<<(std::ostream &, const qualifierst &);
};


class c_qualifierst : public qualifierst
{
public:
  c_qualifierst()
  {
    clear();
  }

  explicit c_qualifierst(const typet &src)
  {
    clear();
    read(src);
  }

protected:
  c_qualifierst &operator=(const c_qualifierst &other);
public:
  virtual std::unique_ptr<qualifierst> clone() const override;

  virtual void clear() override
  {
    is_constant=false;
    is_volatile=false;
    is_restricted=false;
    is_atomic=false;
    is_ptr32=is_ptr64=false;
    is_transparent_union=false;
    is_nodiscard = false;
    is_noreturn=false;
  }

  // standard ones
  bool is_constant, is_volatile, is_restricted, is_atomic, is_noreturn,
    is_nodiscard;

  // MS Visual Studio extension
  bool is_ptr32, is_ptr64;

  // gcc extension
  bool is_transparent_union;

  // will likely add alignment here as well

  virtual std::string as_string() const override;
  virtual void read(const typet &src) override;
  virtual void write(typet &src) const override;

  static void clear(typet &dest);

  bool is_subset_of(const c_qualifierst &other) const
  {
    return (!is_constant || other.is_constant) &&
           (!is_volatile || other.is_volatile) &&
           (!is_restricted || other.is_restricted) &&
           (!is_atomic || other.is_atomic) && (!is_ptr32 || other.is_ptr32) &&
           (!is_ptr64 || other.is_ptr64) &&
           (!is_nodiscard || other.is_nodiscard) &&
           (!is_noreturn || other.is_noreturn);

    // is_transparent_union isn't checked
  }

  bool operator==(const c_qualifierst &other) const
  {
    return is_constant == other.is_constant &&
           is_volatile == other.is_volatile &&
           is_restricted == other.is_restricted &&
           is_atomic == other.is_atomic && is_ptr32 == other.is_ptr32 &&
           is_ptr64 == other.is_ptr64 &&
           is_transparent_union == other.is_transparent_union &&
           is_nodiscard == other.is_nodiscard &&
           is_noreturn == other.is_noreturn;
  }

  bool operator!=(const c_qualifierst &other) const
  {
    return !(*this == other);
  }

  c_qualifierst &operator+=(const c_qualifierst &other)
  {
    is_constant |= other.is_constant;
    is_volatile |= other.is_volatile;
    is_restricted |= other.is_restricted;
    is_atomic |= other.is_atomic;
    is_ptr32 |= other.is_ptr32;
    is_ptr64 |= other.is_ptr64;
    is_transparent_union |= other.is_transparent_union;
    is_nodiscard |= other.is_nodiscard;
    is_noreturn |= other.is_noreturn;
    return *this;
  }

  virtual std::size_t count() const override
  {
    return is_constant + is_volatile + is_restricted + is_atomic + is_ptr32 +
           is_ptr64 + is_nodiscard + is_noreturn;
  }
};

#endif // CPROVER_ANSI_C_C_QUALIFIERS_H
