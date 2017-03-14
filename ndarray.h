#ifndef NDARRAY_H
#define NDARRAY_H
#include <iostream>
#include <initializer_list>
using namespace std;
/**
 * @file ndarray.h
 * @author zpj
 * @brief The n-Dimensional array template for n>1
 * @todo
 * + broadcasting?
 * + Math operations
 * + Make dimension a template argument
 *
 * Should have a view class for slicing?
 */
template <typename dtype, int D>
/// The n-Dimensional C-style array template class
class ndarray {
    static_assert(D > 0, "Dimension should be positive!");
  public:
    using size_t=int32_t;
    using iterator=dtype *;
    using const_iterator=const dtype *;
  private:
    size_t *_shape;         ///< shape of each dimension
    /**
     * @brief stride of every axis.
     *
     * For the purpose of transposition,
     * the naive stride of last index is reserved.
     * So the last stride may be non-zero. Be cautious!
     */
    size_t *_stride;
    void check() {
        for(int i = 0; i < D; i++) {
            if(_shape[i] <= 0) {
                cerr << "Error: Positive shape needed!" << endl;
                exit(0);
            }
        }
    }

  public:
    dtype *head;        ///< head of the array data
    ~ndarray() {
        delete [] head;
        delete [] _shape;
        delete [] _stride;
    }
    /// Constructor an empty ndarray
    ndarray(): _shape(nullptr), _stride(nullptr), head(nullptr) {}
    /**
     * @brief Initialize an ndarray for a square matrix
     * @param width
     */
    explicit ndarray(size_t width): _shape(new size_t[D]), _stride(new size_t[D + 1]) {
        _stride[D] = 1;
        for(int i = 0; i < D; i++) {
            _shape[i] = width;
            _stride[D - i - 1] = _stride[D - i] * width;
        }
        check();
        head = new dtype[size()];
    }
    /**
     * @brief A ndarray with a shape given by pointer/array
     * @param sh    shape array
     *
     * For dynamic construction
     */
    explicit ndarray(const size_t *sh): _shape(new size_t[D]), _stride(new size_t[D + 1]) {
        _stride[D] = 1;
        for(int i = 0; i < D; i++) {
            _shape[i] = sh[i];
            _stride[D - i - 1] = _stride[D - i] * sh[D - i - 1];
        }
        check();
        head = new dtype[size()];
    }
    /**
     * @brief Initialize an ndarray with a shape given by an initializer list
     * @param l     initializer list for shape
     *
     * For static construction
     */
    explicit ndarray(initializer_list<size_t> l): _shape(new size_t[D]), _stride(new size_t[D + 1]) {
        if(l.size() != D){
            cerr<<"Initialization of shape failed."<<endl;
            exit(0);
        }
        copy(std::begin(l), std::end(l), _shape);
        _stride[D] = 1;
        for(int i = 0; i < D; i++) {
            _stride[D - i - 1] = _stride[D - i] * _shape[D - i - 1];
        }
        check();
        head = new dtype[size()];
    }

    /**
     * @brief Copy constructor copying the shape only.
     * @param x     The source of ndarray
     */
    template<typename T>
    ndarray(const ndarray<T, D> & x): _shape(nullptr), _stride(nullptr), head(nullptr) {
        if(!x.empty()) {
            _shape = new size_t[D];
            _stride = new size_t[D + 1];
            for(int i = 0; i < D; i++) {
                _shape[i] = x.shape(i);
                _stride[i + 1] = x.stride(i);
            }
            _stride[0] = x.size();
            head = new dtype[x.size()];
            for(size_t i = 0; i < size(); i++) {
                head[i] = x[i];
            }
        }
    }
    /// Move constructor
    ndarray(ndarray<dtype, D> && x): _shape(x._shape), head(x.head), _stride(x._stride) {
        x._shape = nullptr;
        x.head = nullptr;
        x._stride = nullptr;
    }
    /**
     * @brief Copy Assignment copying all data
     * @param x     The source of ndarray
     */
    ndarray<dtype, D> & operator= (const ndarray<dtype, D>& x) {
        if(this != &x) {
            ndarray<dtype, D> copy = x;
            swap(*this, copy);
        }
        return *this;
    }
    /// Move assignment
    ndarray<dtype, D> & operator= (ndarray<dtype, D>&& x) {
        swap(_shape, x._shape);
        swap(_stride, x._stride);
        swap(head, x.head);
        return *this;
    }
    /**
     * @brief Set all the values in the array to a init value
     * @param val   initial value
     */
    ndarray<dtype, D> & operator= (dtype val) {
        if(empty())
            return *this;
        for(size_t i = 0; i < size(); i++)
            head[i] = val;
        return *this;
    }
    bool empty() const {
        return (_stride == nullptr);
    }
    size_t size() const {
        return *_stride;
    }
    int dim() const {
        return D;
    }
    size_t shape(int i) const {
        return _shape[i];
    }
    const size_t * shape() const {
        return _shape;
    }
    size_t stride(int i) const {
        return _stride[i + 1];
    }
    const size_t * stride() const {
        return _stride + 1;
    }

    /**
     * @brief Naive indexing using raw index
     * @param rawind    raw index
     *
     * + Good efficiency!
     * + If dim==1, use [] rather than ()
     */
    dtype & operator[](size_t rawind) {
        return *(head + rawind);
    }
    const dtype & operator[](size_t rawind) const {
        return *(head + rawind);
    }

    /**
     * @brief Indexing by an index array
     * @param coo   the coordinates of an element
     * @return The indexed element
     */
    dtype & operator()(const size_t *coo) {
        size_t offset = 0;
        for(int i = 0; i < D; i++) {
            offset += coo[i] * _stride[i + 1];
        }
        return head[offset];
    }

    size_t adder(size_t *p, size_t last) const {
        return *p * last;
    }
    template<typename... Args>
    size_t adder(size_t *p, size_t first, Args... args) const {
        return *p * first + adder(p + 1, args...);
    }

    /**
     * @brief Indexing by the argument list of operator()
     * @param args the index of an element
     *
     * + Very useful for array with known dimension.
     * + Emulate the original C style array
     * + If _dim==1, use [] directly rather than ()
     * + If args are not enough, suppose we fill it with 0
     *
     * @return The indexed element
     */
    template<typename... Args>
    dtype & operator()(Args... args) {
        return *(head + adder(_stride + 1, args...));
    }
    template<typename... Args>
    const dtype & operator()(Args... args) const {
        return *(head + adder(_stride + 1, args...));
    }

    template<bool dir>
    /**
     * @brief Roll the index in a given axis with periodical boundary condition
     * @param rawind raw index
     * @param axis
     * @param dir   positive/negative
     * + For positive axis: `0, 1,..., dim-1`
     * + For corresponding negative axis: `-dim, 1-dim,..., -1`
     * @return Raw index after rolling
     */
    size_t rollindex(size_t rawind, int axis) const {
        size_t axisind = (rawind % _stride[axis]) / _stride[axis + 1];
        rawind += (2*dir-1)*_stride[axis + 1];
        if(axisind == dir*(_shape[axis]-1))
            rawind -= (2*dir-1)*_stride[axis];
        return rawind;
    }
    /**
     * @brief roll ind for unknow positiveness by using rollindex
     */
    size_t rollind(size_t rawind, int ax) const {
        if(ax < 0) {
            return rollindex<false>(rawind, ax + D);
        } else {
            return rollindex<true>(rawind, ax);
        }
    }

    /// Transpose between two axis
    void transpose(int i = 1, int j = 0) {
        swap(_stride[i + 1], _stride[j + 1]);
        swap(_shape[i + 1], _shape[j + 1]);
    }
    int size_attached() const { ///< empty condition
        int s = sizeof(ndarray<dtype, D>);
        s += sizeof(size_t) * (2 * D + 1);
        return s;
    }
    ndarray<dtype, D>& operator-() const{
        ndarray<dtype, D> tmp(*this);
        for(size_t i=0;i<size();i++){
            tmp[i]=-head[i];
        }
        return *this;
    }
    template<int N>
    /**
     * Adding rhs with some constexpr coefficient
     * Also foundation for plus, minus
     */
    ndarray<dtype, D>& pm(const ndarray<dtype, D> &rhs){
        for(size_t i = 0; i < size(); i++)
            head[i] += N*rhs[i];
        return *this;
    }
    ndarray<dtype, D>& operator+=(const ndarray<dtype, D> &rhs){
        return this->pm<1>(rhs);
    }
    ndarray<dtype, D>& operator-=(const ndarray<dtype, D> &rhs){
        return *this->pm<-1>(rhs);
    }
    ///< Not using += and -= directly because of the copying cost
    ndarray<dtype, D>& operator+(const ndarray<dtype, D> &rhs) const {
        ndarray<dtype, D> tmp(*this);
        for(size_t i=0;i<size();i++){
            tmp[i]=head[i]+rhs[i];
        }
        return tmp;
    }
    ndarray<dtype, D>& operator-(const ndarray<dtype, D> &rhs) const {
        ndarray<dtype, D> tmp(*this);
        for(size_t i=0;i<size();i++){
            tmp[i]=head[i]-rhs[i];
        }
        return tmp;
    }
    iterator begin() const {
        return head;
    }
    iterator end() const {
        return head+size();
    }
    const_iterator cbegin() const {
        return head;
    }
    const_iterator cend() const {
        return head+size();
    }
};

template<class dtype>
class matrix: public ndarray<dtype, 2> {
  public:
    using ndarray<dtype, 2>::operator=;
    using ndarray<dtype, 2>::ndarray;
    void print() {
        for(int i = 0; i < this->shape(0); i++) {
            for(int j = 0; j < this->shape(1); j++) {
                cout << (*this)(i, j) << '\t';
            }
            cout << endl;
        }
    }
    matrix<dtype> operator*(matrix<dtype> &rhs) {
        matrix<dtype> c{this->shape(0), rhs.shape(1)};
        c = 0;
        if(this->shape(1) != rhs.shape(0)) {
            cerr << "Dimension not match!" << endl;
            return c;
        }
        for(int i = 0; i < c.shape(0); i++) {
            for(int j = 0; j < c.shape(1); j++) {
                for(int k = 0; k < this->shape(1); k++) {
                    c(i, j) += (*this)(i, k) * rhs(k, j);
                }
            }
        }
        return c;
    }
    int nrow() const {
        return this->empty() ? 0 : this->shape(0);
    }
    int ncol() const {
        return this->empty() ? 0 : this->shape(1);
    }
};

#endif //NDARRAY_H
