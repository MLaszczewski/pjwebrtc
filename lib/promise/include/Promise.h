//
// Created by Michał Łaszczewski on 13/02/17.
//

#ifndef PROMISE_H
#define PROMISE_H

#include <functional>
#include <exception>
#include <memory>
#include <vector>
#include <string>

namespace promise {

  template<typename T> class Promise : public std::enable_shared_from_this<Promise<T>> {
  public:
    enum class PromiseState {
      Pending = 0,
      Resolved = 1,
      Rejected = 2
    };

    using ResolveCallback = std::function<void(T& result)>;
    using RejectCallback = std::function<void(std::exception_ptr exception)>;

    PromiseState state;

    T result;
    std::exception_ptr exception;

    std::vector<ResolveCallback> resolveCallbacks;
    std::vector<RejectCallback> rejectCallbacks;

    Promise() : state(PromiseState::Pending) {
    }
    ~Promise() {}

    void run(std::function<void(std::shared_ptr<Promise>)> fun) {
      try {
        fun(this->shared_from_this());
      } catch (...) {
        reject(std::current_exception());
      }
    }

    void resolve(T resultp) {
      state = PromiseState::Resolved;
      result = resultp;
      for(auto& cb : resolveCallbacks) {
        cb(result);
      }
      resolveCallbacks.clear();
      rejectCallbacks.clear();
    }
    void reject(std::exception_ptr exceptionp) {
      if(state == PromiseState::Rejected) return; // already rejected
      state = PromiseState::Rejected;
      exception = exceptionp;
      for(auto& cb : rejectCallbacks) {
        cb(exception);
      }
      resolveCallbacks.clear();
      if(rejectCallbacks.size() == 0) {
        rejectCallbacks.clear();
        std::rethrow_exception(exceptionp);
      }
      rejectCallbacks.clear();
    }

    void chain(std::shared_ptr<Promise<T>> to) {
      onRejected([to](std::exception_ptr ex){
        to->reject(ex);
      });
      onResolved([to](T res){
        to->resolve(res);
      });
    }

    void onResolved(ResolveCallback callback) {
      if(state == PromiseState::Pending) {
        resolveCallbacks.push_back(callback);
      } else if(state == PromiseState::Resolved) {
        callback(result);
      }
    }
    void onRejected(RejectCallback callback) {
      if(state == PromiseState::Pending) {
        rejectCallbacks.push_back(callback);
      } else if(state == PromiseState::Rejected) {
        callback(exception);
      }
    }

    template<typename R> std::shared_ptr<Promise<R>> then(std::function<std::shared_ptr<Promise<R>>(T& result)> fun) {
      auto res = std::make_shared<Promise<R>>();
      onResolved([res, fun](T& result){
        fun(result)->chain(res);
      });
      onRejected([res](std::exception_ptr exceptionp) {
        res->reject(exceptionp);
      });
      return res;
    }

    template<typename R> std::shared_ptr<Promise<R>> then(std::function<R(T& result)> fun) {
      auto res = std::make_shared<Promise<R>>();
      onResolved([res, fun](T& result){
        try {
          res->resolve(fun(result));
        } catch(...) {
          res->reject(std::current_exception());
        }
      });
      onRejected([res](std::exception_ptr exceptionp) {
        res->reject(exceptionp);
      });
      return res;
    }

    template<typename R> std::shared_ptr<Promise<R>> then(std::function<std::shared_ptr<Promise<R>>(T& result)> fun,
                                                          std::function<std::shared_ptr<Promise<R>>(std::exception_ptr exception)> err) {
      auto res = std::make_shared<Promise<R>>();
      onResolved([res, fun](T& result){
        fun(result)->chain(res);
      });
      onRejected([res, err](std::exception_ptr exceptionp) {
        err(exceptionp)->chain(res);
      });
      return res;
    }

    template<typename R> std::shared_ptr<Promise<R>> then(std::function<std::shared_ptr<Promise<R>>(T result)> fun,
                                                          std::function<void(std::exception_ptr exception)> err) {
      auto res = std::make_shared<Promise<R>>();
      onResolved([res, fun](T& result){
        fun(result)->chain(res);
      });
      onRejected(err);
      onRejected([res](std::exception_ptr exceptionp) {
        res->reject(exceptionp);
      });
      return res;
    }
    template<typename R> std::shared_ptr<Promise<R>> then(std::function<void(T& result)> fun,
                                                           std::function<void(std::exception_ptr exception)> err) {
      auto res = std::make_shared<Promise<R>>();
      onResolved(fun);
      onRejected(err);
      chain(res);
      return res;
    }

    template<typename R> std::shared_ptr<Promise<R>> grab(std::function<void(std::exception_ptr exception)> err) {
      auto res = std::make_shared<Promise<R>>();
      onRejected(err);
      chain(res);
      return res;
    }

    template<typename R> std::shared_ptr<Promise<R>> grab(std::function<std::shared_ptr<Promise<R>> (std::exception_ptr exception)> err) {
      auto res = std::make_shared<Promise<R>>();
      onRejected([res, err](std::exception_ptr exceptionp){
        err(exceptionp)->chain(res);
      });
      onResolved([res](T& result) {
        res->resolve(result);
      });
      return res;
    }

    static std::shared_ptr<Promise<T>> resolved(T result) {
      auto p = std::make_shared<Promise<T>>();
      p->resolve(result);
      return p;
    }
    static std::shared_ptr<Promise<T>> rejected(std::exception_ptr exceptionp) {
      auto p = std::make_shared<Promise<T>>();
      p->reject(exceptionp);
      return p;
    }
  };

}

#endif //TANKS_PROMISE_H

