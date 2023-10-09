#ifndef MODULES_V4D_SRC_BACKEND_HPP_
#define MODULES_V4D_SRC_BACKEND_HPP_

#include "context.hpp"

#include <tuple>
#include <functional>
#include <utility>
#include <type_traits>
#include <opencv2/core.hpp>


//based on https://stackoverflow.com/a/61576038/1884837
namespace cv {
namespace v4d {

class Transaction {
private:
	cv::Ptr<cv::v4d::detail::V4DContext> ctx_;
	std::function<bool()> condition_;
public:
	virtual ~Transaction() {}
    virtual void perform() = 0;
    virtual bool enabled() = 0;
    virtual bool hasCondition() = 0;

    void setContext(cv::Ptr<cv::v4d::detail::V4DContext> ctx) {
    	ctx_ = ctx;
    }
    cv::Ptr<cv::v4d::detail::V4DContext> getContext() {
    	return ctx_;
    }
};

namespace detail {

template <typename F, typename... Ts>
class TransactionImpl : public Transaction
{
    static_assert(sizeof...(Ts) == 0 || (!(std::is_rvalue_reference_v<Ts> && ...)));
private:
    F f;
    std::tuple<Ts...> args;
public:
    template <typename FwdF, typename... FwdTs,
        typename = std::enable_if_t<sizeof...(Ts) == 0 || ((std::is_convertible_v<FwdTs&&, Ts> && ...))>>
		TransactionImpl(FwdF&& func, FwdTs&&... fwdArgs)
        : f(std::forward<FwdF>(func)),
          args{std::forward_as_tuple(fwdArgs...)}
    {}

    virtual ~TransactionImpl() override
	{}

    virtual void perform() override
    {
        std::apply(f, args);
    }

    template<bool b>
    typename std::enable_if<b, bool>::type enabled() {
    	return std::apply(f, args);
    }

    template<bool b>
    typename std::enable_if<!b, bool>::type enabled() {
    	return false;
    }

    virtual bool enabled() override {
    	return enabled<std::is_same_v<std::remove_cv_t<typename decltype(f)::result_type>, bool>>();
    }

    template<bool b>
    typename std::enable_if<b, bool>::type hasCondition() {
    	return true;
    }

    template<bool b>
    typename std::enable_if<!b, bool>::type hasCondition() {
    	return false;
    }

    virtual bool hasCondition() override {
    	return hasCondition<std::is_same_v<std::remove_cv_t<typename decltype(f)::result_type>, bool>>();
    }
};
}

template <typename F, typename... Args>
cv::Ptr<Transaction> make_transaction(F f, Args&&... args) {
    return cv::Ptr<Transaction>(dynamic_cast<Transaction*>(new detail::TransactionImpl<std::decay_t<F>, std::remove_cv_t<Args>...>
        (std::forward<F>(f), std::forward<Args>(args)...)));
}


template <typename F, typename Tfb, typename... Args>
cv::Ptr<Transaction> make_transaction(F f, Tfb&& fb, Args&&... args) {
	return cv::Ptr<Transaction>(dynamic_cast<Transaction*>(new detail::TransactionImpl<std::decay_t<F>, std::remove_cv_t<Tfb>, std::remove_cv_t<Args>...>
        (std::forward<F>(f), std::forward<Tfb>(fb), std::forward<Args>(args)...)));
}


}
}

#endif /* MODULES_V4D_SRC_BACKEND_HPP_ */
