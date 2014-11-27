#include <stdio.h>
#include <iostream>
#include <string>
#include <string.h>
#include <node_buffer.h>
#include "Types.h"
#include "request.h"
#include "ScriptEncapsulated-v10.h"

using namespace std;
using namespace v8;

Handle<Value> callback(HandleScope &scope,const Local<Function> &cb,int argc,Local<Value> *argv)
{
   cb->Call(Context::GetCurrent()->Global(),argc,argv);
   return scope.Close(Undefined());
}

Local<Value> createPrimitiveRes(HandleScope &scope,const nj::Primitive &primitive)
{

   switch(primitive.type()->getId())
   {
      case nj::null_type:
      {
         Local<Value> dest = Local<Value>::New(Null());

         return dest;
      }
      break;
      case nj::boolean_type:
      {
         Local<Value> dest = Boolean::New(primitive.toBoolean())->ToBoolean();

         return dest;
      }
      break;
      case nj::int64_type:
      case nj::int32_type:
      case nj::int16_type:
      case nj::int8_type:
      {
         Local<Value> dest = Number::New(primitive.toInt());

         return dest;
      }
      break;
      case nj::uint64_type:
      case nj::uint32_type:
      case nj::uint16_type:
      case nj::uint8_type:
      {
         Local<Value> dest = Number::New(primitive.toUInt());

         return dest;
      }
      break;
      case nj::float64_type:
      case nj::float32_type:
      {
         Local<Value> dest = Number::New(primitive.toFloat());

         return dest;
      }
      break;
      case nj::ascii_string_type:
      case nj::utf8_string_type:
      {
         Local<Value> dest = String::New(primitive.toString().c_str());

         return dest;
      }
      break;
      case nj::date_type:
      {
         Local<Value> dest = Date::New(primitive.toFloat());

         return dest;
      }
      break;
      case nj::regex_type:
      {
         Local<String> pattern = String::New(primitive.toString().c_str());
         Local<Value> dest = RegExp::New(pattern,RegExp::kNone);

         return dest;
      }
      break;
   }

   return Local<Value>::New(Null());
}

template <typename V> Local<Value> getNumberFromValue(const V &val)
{
   return Number::New(val);
}

Local<Value> getStringFromValue(const string &val)
{
   return String::New(val.c_str());
}

Local<Value> getDateFromValue(const double &val)
{
   return Date::New(val);
}

Local<Value> getNullValue(const unsigned char &val)
{
   return Local<Value>::New(Null());
}


template<typename V,Local<Value> getElement(const V &val)> static void connectSubArrays(const vector<size_t> &dims,const vector<size_t> &strides,size_t ixNum,size_t offset,const Local<Array> &to,V *from)
{
   size_t numElements = dims[ixNum];
   size_t stride = strides[ixNum];

   if(ixNum == dims.size() - 1)
   {
      for(size_t elementNum = 0;elementNum < numElements;elementNum++)
      {
         to->Set(elementNum,getElement(*(from + offset)));
         offset += stride;
      }
   }
   else
   {
      for(size_t elementNum = 0;elementNum < numElements;elementNum++)
      {
         Local<Array> subArray = Array::New(dims[ixNum + 1]);

         to->Set(elementNum,subArray);
         connectSubArrays<V,getElement>(dims,strides,ixNum + 1,offset,subArray,from);
         offset += stride;
      }
   }
}

template<typename V,typename E,Local<Value> getElement(const V &val)> Local<Array> createArrayRes(HandleScope &scope,const shared_ptr<nj::Value> &value)
{
   const nj::Array<V,E> &array = static_cast<const nj::Array<V,E>&>(*value);

   if(array.size() == 0) return Local<Array>();
   if(array.dims().size() == 1)
   {
      size_t size0 = array.dims()[0];
      V *p = array.ptr();
      Local<Array> dest = Array::New(size0);

      for(size_t i = 0;i < size0;i++) dest->Set(i,getElement(p[i]));
      return dest;
   }
   else if(array.dims().size() == 2)
   {
      size_t size0 = array.dims()[0];
      size_t size1 = array.dims()[1];
      V *p = array.ptr();
      Local<Array> dest = Array::New(size0);

      for(size_t i = 0;i < size0;i++)
      {
         Local<Array> row  = Array::New(size1);

         dest->Set(i,row);
         for(size_t j = 0;j < size1;j++) row->Set(j,getElement(p[size0*j + i]));
      }
      return dest;
   }
   else
   {
      vector<size_t> strides;
      size_t size0 = array.dims()[0];
      Local<Array> dest = Array::New(size0);

      strides.push_back(1);
      for(size_t idxNum = 1;idxNum < array.dims().size();idxNum++) strides.push_back(array.dims()[idxNum]*strides[idxNum - 1]);
      connectSubArrays<V,getElement>(array.dims(),strides,0,0,dest,array.ptr());
      return dest;
   }
   return Array::New(0);
}

Local<Array> createArrayRes(HandleScope &scope,const shared_ptr<nj::Value> &value,const nj::Type *elementType)
{
   switch(elementType->getId())
   {
      case nj::null_type: return createArrayRes<unsigned char,nj::Null_t,getNullValue>(scope,value); break;
      case nj::float64_type: return createArrayRes<double,nj::Float64_t,getNumberFromValue<double>>(scope,value); break;
      case nj::float32_type: return createArrayRes<float,nj::Float32_t,getNumberFromValue<float>>(scope,value); break;
      case nj::int64_type: return createArrayRes<int64_t,nj::Int64_t,getNumberFromValue<int64_t>>(scope,value); break;
      case nj::int32_type: return createArrayRes<int,nj::Int32_t,getNumberFromValue<int>>(scope,value); break;
      case nj::int16_type: return createArrayRes<short,nj::Int16_t,getNumberFromValue<short>>(scope,value); break;
      case nj::int8_type: return createArrayRes<char,nj::Int8_t,getNumberFromValue<char>>(scope,value); break;
      case nj::uint64_type: return createArrayRes<uint64_t,nj::UInt64_t,getNumberFromValue<uint64_t>>(scope,value); break;
      case nj::uint32_type: return createArrayRes<unsigned,nj::UInt32_t,getNumberFromValue<unsigned>>(scope,value); break;
      case nj::uint16_type: return createArrayRes<unsigned short,nj::UInt16_t,getNumberFromValue<unsigned short>>(scope,value); break;
      case nj::uint8_type: return createArrayRes<unsigned char,nj::UInt8_t,getNumberFromValue<unsigned char>>(scope,value); break;
      case nj::ascii_string_type: return createArrayRes<string,nj::ASCIIString_t,getStringFromValue>(scope,value); break;
      case nj::utf8_string_type: return createArrayRes<string,nj::UTF8String_t,getStringFromValue>(scope,value); break;
      case nj::date_type: return createArrayRes<double,nj::Date_t,getDateFromValue>(scope,value); break;
   }

   return Array::New(0);
}

Local<Object> createBufferRes(HandleScope &scope,const shared_ptr<nj::Value> &value)
{
   const nj::Array<unsigned char,nj::UInt8_t> &array = static_cast<const nj::Array<unsigned char,nj::UInt8_t>&>(*value);

   size_t length = array.dims()[0];
   unsigned char *p = array.ptr();
   node::Buffer *buffer = node::Buffer::New(length);

   memcpy(node::Buffer::Data(buffer),p,length);

   Local<Object> globalObj = Context::GetCurrent()->Global();
   Local<Function> cons = Local<Function>::Cast(globalObj->Get(String::New("Buffer")));
   Handle<Value> cArgs[3] = { buffer->handle_, v8::Integer::New(length), v8::Integer::New(0) };

   return cons->NewInstance(3,cArgs);
}

int createResponse(HandleScope &scope,const shared_ptr<nj::Result> &res,int argc,Local<Value> *argv)
{
   int index = 0;

   for(shared_ptr<nj::Value> value: res->results())
   {
      if(value.get())
      {
         if(value->isPrimitive())
         {
            const nj::Primitive &primitive = static_cast<const nj::Primitive&>(*value);

            argv[index++] = createPrimitiveRes(scope,primitive);
         }
         else
         {
            const nj::Array_t *array_type = static_cast<const nj::Array_t*>(value->type());
            const nj::Type *etype = array_type->etype();

            if(etype == nj::UInt8_t::instance()  && value->dims().size() == 1) argv[index++] = createBufferRes(scope,value);
            else argv[index++] = createArrayRes(scope,value,etype);
         }
      }
   }
   return index;
}

Handle<Value> raiseException(HandleScope &scope,shared_ptr<nj::Result> &res)
{
   int exId = res->exId();

   switch(exId)
   {
      case nj::Exception::julia_undef_var_error_exception:
      case nj::Exception::julia_method_error_exception:
         ThrowException(Exception::ReferenceError(String::New(res->exText().c_str())));
      break;
      default:
         ThrowException(Exception::Error(String::New(res->exText().c_str())));
      break;
   }
   return scope.Close(Undefined());
}

Handle<Value> callbackWithResult(HandleScope &scope,Local<Function> &cb,shared_ptr<nj::Result> &res)
{
   if(res.get())
   {
      int exId = res->exId();

      if(exId != nj::Exception::no_exception) return raiseException(scope,res);
      else
      {
         int argc = res->results().size();
         Local<Value> *argv = new Local<Value>[argc];

         argc = createResponse(scope,res,argc,argv);
         return callback(scope,cb,argc,argv);
      }
   }
   else return callback(scope,cb,0,0);
}

Handle<Value> returnResult(HandleScope &scope,shared_ptr<nj::Result> &res)
{
   if(res.get())
   {
      int exId = res->exId();

      if(exId != nj::Exception::no_exception) return raiseException(scope,res);
      else
      {
         int argc = res->results().size();
         Local<Value> *argv = new Local<Value>[argc];

         argc = createResponse(scope,res,argc,argv);

         if(argc != 0)
         {
            if(argc > 1)
            {
               Local<Array> rV = Array::New(argc);

               for(int i = 0;i < argc;i++) rV->Set(i,argv[i]);

               return scope.Close(rV);
            }
            else return scope.Close(argv[0]);
         }
         else return scope.Close(Null());
      }
   }
   else return scope.Close(Undefined());
}

Handle<Value> doEval(const Arguments &args)
{
   HandleScope scope;
   int numArgs = args.Length();

   if(numArgs  == 0 || numArgs > 2 || (numArgs == 2 && !args[1]->IsFunction())) return scope.Close(Null());
   if(!J) J = new JuliaExecEnv();

   Local<String> arg0 = Local<String>::Cast(args[0]);
   Local<Function> cb;
   String::Utf8Value text(arg0);
   JMain *engine;

   if(numArgs == 2) cb = Local<Function>::Cast(args[1]);

   if(text.length() > 0 && (engine = J->getEngine()))
   {
      engine->eval(*text);
      shared_ptr<nj::Result> res = engine->resultQueueGet();
 
      if(numArgs == 2) return callbackWithResult(scope,cb,res);
      else return returnResult(scope,res);
   }
   else
   {
      if(numArgs == 2)
      {  
         const unsigned argc = 1;
         Local<Value> argv[argc] = { String::New("") };

         return callback(scope,cb,argc,argv);
      }
      else return scope.Close(Null());
   }
}

Handle<Value> doExec(const Arguments &args)
{
   HandleScope scope;
   int numArgs = args.Length();
   bool useCallback = false;

   if(numArgs == 0) return scope.Close(Null());
   if(!J) J = new JuliaExecEnv();

   Local<String> arg0 = Local<String>::Cast(args[0]);
   String::Utf8Value funcName(arg0);
   Local<Function> cb;
   JMain *engine;

   if(numArgs >= 2 && args[args.Length() - 1]->IsFunction())
   {
      useCallback = true;
      cb = Local<Function>::Cast(args[args.Length() - 1]);
   }

   if(funcName.length() > 0 && (engine = J->getEngine()))
   {
      vector<shared_ptr<nj::Value>> req;
      int numExecArgs = numArgs - 1;

      if(useCallback) numExecArgs--;

      for(int i = 0;i < numExecArgs;i++)
      {
         shared_ptr<nj::Value> reqElement = createRequest(args[i + 1]);

         if(reqElement.get()) req.push_back(reqElement);
      }
      engine->exec(*funcName,req);
      shared_ptr<nj::Result> res = engine->resultQueueGet();

      if(useCallback) return callbackWithResult(scope,cb,res);
      else return returnResult(scope,res);
   }
   else
   {
      if(useCallback)
      {  
         const unsigned argc = 1;
         Local<Value> argv[argc] = { String::New("") };

         return callback(scope,cb,argc,argv);
      }
      else return scope.Close(Null());
   }
}

Handle<Value> newScript(const Arguments& args)
{
  HandleScope scope;

  return scope.Close(nj::ScriptEncapsulated::NewInstance(args));
}

void init(Handle<Object> exports)
{
   nj::ScriptEncapsulated::Init(exports);

   NODE_SET_METHOD(exports,"eval",doEval);
   NODE_SET_METHOD(exports,"exec",doExec);
   NODE_SET_METHOD(exports,"newScript",newScript);
}

NODE_MODULE(nj,init)
