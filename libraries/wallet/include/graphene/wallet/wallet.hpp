/*
 * Copyright (c) 2015 Cryptonomex, Inc., and contributors.
 *
 * The MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <graphene/app/api.hpp>
#include <graphene/utilities/key_conversion.hpp>
#include <graphene/chain/data_market_object.hpp>

using namespace graphene::app;
using namespace graphene::chain;
using namespace graphene::utilities;
using namespace std;

namespace fc
{
   void to_variant(const account_multi_index_type& accts, variant& vo);
   void from_variant(const variant &var, account_multi_index_type &vo);
}

namespace graphene { namespace wallet {

typedef uint16_t transaction_handle_type;

/**
 * This class takes a variant and turns it into an object
 * of the given type, with the new operator.
 */

object* create_object( const variant& v );

struct plain_keys
{
   map<public_key_type, string>  keys;
   fc::sha512                    checksum;
};

struct brain_key_info
{
   string brain_priv_key;
   string wif_priv_key;
   public_key_type pub_key;
};


/**
 *  Contains the confirmation receipt the sender must give the receiver and
 *  the meta data about the receipt that helps the sender identify which receipt is
 *  for the receiver and which is for the change address.
 */
struct blind_confirmation 
{
   struct output
   {
      string                          label;
      public_key_type                 pub_key;
      stealth_confirmation::memo_data decrypted_memo;
      stealth_confirmation            confirmation;
      authority                       auth;
      string                          confirmation_receipt;
   };

   signed_transaction     trx;
   vector<output>         outputs;
};

struct blind_balance
{
   asset                     amount;
   public_key_type           from; ///< the account this balance came from
   public_key_type           to; ///< the account this balance is logically associated with
   public_key_type           one_time_key; ///< used to derive the authority key and blinding factor
   fc::sha256                blinding_factor;
   fc::ecc::commitment_type  commitment;
   bool                      used = false;
};

struct blind_receipt
{
   std::pair<public_key_type,fc::time_point>        from_date()const { return std::make_pair(from_key,date); }
   std::pair<public_key_type,fc::time_point>        to_date()const   { return std::make_pair(to_key,date);   }
   std::tuple<public_key_type,asset_id_type,bool>   to_asset_used()const   { return std::make_tuple(to_key,amount.asset_id,used);   }
   const commitment_type& commitment()const        { return data.commitment; }

   fc::time_point                  date;
   public_key_type                 from_key;
   string                          from_label;
   public_key_type                 to_key;
   string                          to_label;
   asset                           amount;
   string                          memo;
   authority                       control_authority;
   stealth_confirmation::memo_data data;
   bool                            used = false;
   stealth_confirmation            conf;
};

struct by_from;
struct by_to;
struct by_to_asset_used;
struct by_commitment;

typedef multi_index_container< blind_receipt,
   indexed_by<
      ordered_unique< tag<by_commitment>, const_mem_fun< blind_receipt, const commitment_type&, &blind_receipt::commitment > >,
      ordered_unique< tag<by_to>, const_mem_fun< blind_receipt, std::pair<public_key_type,fc::time_point>, &blind_receipt::to_date > >,
      ordered_non_unique< tag<by_to_asset_used>, const_mem_fun< blind_receipt, std::tuple<public_key_type,asset_id_type,bool>, &blind_receipt::to_asset_used > >,
      ordered_unique< tag<by_from>, const_mem_fun< blind_receipt, std::pair<public_key_type,fc::time_point>, &blind_receipt::from_date > >
   >
> blind_receipt_index_type;


struct key_label
{
   string          label;
   public_key_type key;
};


struct by_label;
struct by_key;
typedef multi_index_container<
   key_label,
   indexed_by<
      ordered_unique< tag<by_label>, member< key_label, string, &key_label::label > >,
      ordered_unique< tag<by_key>, member< key_label, public_key_type, &key_label::key > >
   >
> key_label_index_type;


struct wallet_data
{
   /** Chain ID this wallet is used with */
   chain_id_type chain_id;
   account_multi_index_type my_accounts;
   /// @return IDs of all accounts in @ref my_accounts
   vector<object_id_type> my_account_ids()const
   {
      vector<object_id_type> ids;
      ids.reserve(my_accounts.size());
      std::transform(my_accounts.begin(), my_accounts.end(), std::back_inserter(ids),
                     [](const account_object& ao) { return ao.id; });
      return ids;
   }
   /// Add acct to @ref my_accounts, or update it if it is already in @ref my_accounts
   /// @return true if the account was newly inserted; false if it was only updated
   bool update_account(const account_object& acct)
   {
      auto& idx = my_accounts.get<by_id>();
      auto itr = idx.find(acct.get_id());
      if( itr != idx.end() )
      {
         idx.replace(itr, acct);
         return false;
      } else {
         idx.insert(acct);
         return true;
      }
   }

   /** encrypted keys */
   vector<char>              cipher_keys;

   /** map an account to a set of extra keys that have been imported for that account */
   map<account_id_type, set<public_key_type> >  extra_keys;

   // map of account_name -> base58_private_key for
   //    incomplete account regs
   map<string, vector<string> > pending_account_registrations;
   map<string, string> pending_witness_registrations;

   key_label_index_type                                              labeled_keys;
   blind_receipt_index_type                                          blind_receipts;

   string                    ws_server = "ws://localhost:8090";
   string                    ws_user;
   string                    ws_password;
};

struct exported_account_keys
{
    string account_name;
    vector<vector<char>> encrypted_private_keys;
    vector<public_key_type> public_keys;
};

struct exported_keys
{
    fc::sha512 password_checksum;
    vector<exported_account_keys> account_keys;
};

struct approval_delta
{
   vector<string> active_approvals_to_add;
   vector<string> active_approvals_to_remove;
   vector<string> owner_approvals_to_add;
   vector<string> owner_approvals_to_remove;
   vector<string> key_approvals_to_add;
   vector<string> key_approvals_to_remove;
};

struct worker_vote_delta
{
   flat_set<worker_id_type> vote_for;
   flat_set<worker_id_type> vote_against;
   flat_set<worker_id_type> vote_abstain;
};

struct vesting_balance_object_with_info : public vesting_balance_object
{
   vesting_balance_object_with_info( const vesting_balance_object& vbo, fc::time_point_sec now );
   vesting_balance_object_with_info( const vesting_balance_object_with_info& vbo ) = default;

   /**
    * How much is allowed to be withdrawn.
    */
   asset allowed_withdraw;

   /**
    * The time at which allowed_withdrawal was calculated.
    */
   fc::time_point_sec allowed_withdraw_time;
};

namespace detail {
class wallet_api_impl;
}

/***
 * A utility class for performing various state-less actions that are related to wallets
 */
class utility {
   public:
      /**
       * Derive any number of *possible* owner keys from a given brain key.
       *
       * NOTE: These keys may or may not match with the owner keys of any account.
       * This function is merely intended to assist with account or key recovery.
       *
       * @see suggest_brain_key()
       *
       * @param brain_key    Brain key
       * @param number_of_desired_keys  Number of desired keys
       * @return A list of keys that are deterministically derived from the brainkey
       */
      static vector<brain_key_info> derive_owner_keys_from_brain_key(string brain_key, int number_of_desired_keys = 1);
};

struct operation_detail {
   string                   memo;
   string                   description;
   operation_history_object op;
};

struct operation_detail_ex {
   string                    memo;
   string                    description;
   operation_history_object op;
   transaction_id_type transaction_id;
};

struct account_history_operation_detail {
   uint32_t                     total_without_operations;
   vector<operation_detail_ex>  details;
};

struct irreversible_account_history_detail {
    uint32_t                     next_start_sequence = 0;
    uint32_t                     result_count = 0;
    vector<operation_detail_ex>  details;
};

/**
 * This wallet assumes it is connected to the database server with a high-bandwidth, low-latency connection and
 * performs minimal caching. This API could be provided locally to be used by a web interface.
 */
class wallet_api
{
   public:
      wallet_api( const wallet_data& initial_data, fc::api<login_api> rapi );
      virtual ~wallet_api();

      bool copy_wallet_file( string destination_filename );

      fc::ecc::private_key derive_private_key(const std::string& prefix_string, int sequence_number) const;



      /**
       * @see create_data_market_category()
       * @brief create_data_market_category
       * @param category_name
       * @param data_market_type
       * @param order_num
       * @param issuer
       * @param broadcast
       * @return
       */
      signed_transaction create_data_market_category(string category_name,uint8_t data_market_type,uint32_t order_num,string issuer,bool broadcast /* = false */);

      /**
       * @brief propose_data_market_category_update
       * @param proposing_account
       * @param category_id
       * @param new_category_name
       * @param new_order_num
       * @param new_status
       * @param broadcast
       * @return
       */
      signed_transaction propose_data_market_category_update(const string &proposing_account,string category_id,string new_category_name,uint32_t new_order_num,uint8_t new_status,bool broadcast);


      /**
       * @brief create_free_data_product
       * @param product_name
       * @param brief_desc
       * @param datasource_account
       * @param category_id
       * @param price
       * @param icon
       * @param schema_contexts
       * @param parent_id
       * @param issuer
       * @param recommend
       * @param broadcast
       * @return
       */
      signed_transaction create_free_data_product(string product_name,string brief_desc,string datasource_account,string category_id,double price,string icon,vector<schema_context_object> schema_contexts,string parent_id,string issuer,bool recommend,bool broadcast);
      /**
       * @brief propose_free_data_product_update
       * @param proposing_account
       * @param free_data_product_id
       * @param new_product_name
       * @param new_brief_desc
       * @param new_datasource_account
       * @param new_category_id
       * @param new_price
       * @param new_icon
       * @param new_schema_contexts
       * @param new_parent_id
       * @param new_status
       * @param new_recommend
       * @param broadcast
       * @return
       */
       signed_transaction propose_free_data_product_update(const string &proposing_account,string free_data_product_id,string new_product_name,string new_brief_desc,string new_datasource_account,string new_category_id,double new_price,string new_icon,vector<schema_context_object> new_schema_contexts,string new_parent_id,uint8_t new_status,bool new_recommend,bool broadcast);

      /**
       * @brief create_league_data_product
       * @param product_name
       * @param brief_desc
       * @param category_id
       * @param refer_price
       * @param icon
       * @param schema_contexts
       * @param issuer
       * @param pocs_threshold
       * @param broadcast
       * @return
       */
      signed_transaction create_league_data_product(string product_name,string brief_desc,string category_id,double refer_price,string icon,vector<schema_context_object> schema_contexts,string issuer,uint64_t pocs_threshold,bool broadcast);
      /**
       * @brief propose_league_data_product_update
       * @param proposing_account
       * @param league_data_product
       * @param new_product_name
       * @param new_brief_desc
       * @param new_category_id
       * @param new_refer_price
       * @param new_icon
       * @param new_schema_contexts
       * @param new_status
       * @param new_pocs_threshold
       * @param broadcast
       * @return
       */
      signed_transaction propose_league_data_product_update(const string &proposing_account,string league_data_product,string new_product_name,string new_brief_desc,string new_category_id,double new_refer_price,string new_icon,vector<schema_context_object> new_schema_contexts,uint8_t new_status,uint64_t new_pocs_threshold,bool broadcast);


      /**
       * @brief create_league
       * @param league_name
       * @param brief_desc
       * @param members
       * @param data_products
       * @param prices
       * @param category_id
       * @param icon
       * @param issuer
       * @param pocs_thresholds
       * @param fee_bases
       * @param pocs_weights
       * @param recommend
       * @param broadcast
       * @return
       */
       signed_transaction create_league(string league_name, string brief_desc, vector<account_id_type> members, vector <league_data_product_id_type> data_products, vector <uint64_t> prices,string category_id, string icon, string issuer, vector<uint64_t> pocs_thresholds, vector<uint64_t> fee_bases, vector<uint64_t> pocs_weights, bool recommend, bool broadcast);

      /**
       * @brief data_transaction_create
       * @param request_id
       * @param product_id
       * @param league_id
       * @param version
       * @param params
       * @param requester
       * @param broadcast
       * @return
       */
       signed_transaction data_transaction_create(string request_id, object_id_type product_id, fc::optional<league_id_type> league_id, string version, string params, string requester, bool broadcast);

       /**
        * @brief pay_data_transaction
        * @param from
        * @param to
        * @param amount
        * @param request_id
        * @param broadcast
        * @return
        */
       signed_transaction pay_data_transaction(string from, string to, asset amount, string request_id, bool broadcast);

       /**
        * @brief data_transaction_datasource_validate_error
        * @param request_id
        * @param datasource
        * @param broadcast
        * @return
        */
       signed_transaction data_transaction_datasource_validate_error(string request_id, string datasource, bool broadcast);

       /** data_transaction_complain_datasource Complaint data fraud.
       *
       * @param request_id
       * @param requester Account id of the requester
       * @param datasource Account id of the datasource
       * @param merchant_copyright_hash
       * @param datasource_copyright_hash
       * @param broadcast true if you wish to broadcast the transaction
       * @return signed_transaction
       */
       signed_transaction data_transaction_complain_datasource(string request_id, account_id_type requester, account_id_type datasource,
         string merchant_copyright_hash, string datasource_copyright_hash, bool broadcast);

       /** list_data_transaction_complain_requesters.
       *
       * @param start_date_time
       * @param end_date_time
       * @param limit
       * @return map<account_id_type, uint64_t>
       */ 
       map<account_id_type, uint64_t> list_data_transaction_complain_requesters(fc::time_point_sec start_date_time, fc::time_point_sec end_date_time, uint8_t limit) const;

       /** list_data_transaction_complain_datasources.
       *
       * @param start_date_time
       * @param end_date_time
       * @param limit
       * @return map<account_id_type, uint64_t>
       */
       map<account_id_type, uint64_t> list_data_transaction_complain_datasources(fc::time_point_sec start_date_time, fc::time_point_sec end_date_time, uint8_t limit) const;

       /**
        * @brief data_transaction_datasource_upload
        * @param request_id
        * @param requester
        * @param datasource
        * @param copyright_hash
        * @param broadcast
        * @return
        */
       signed_transaction data_transaction_datasource_upload(string request_id, string requester, string datasource, fc::optional<string> copyright_hash, bool broadcast);

      /**
       * @brief data_transaction_update
       * @param request_id
       * @param new_status
       * @param new_requester
       * @param memo
       * @param broadcast
       * @return
       */
       signed_transaction data_transaction_update(string request_id, uint8_t new_status, string new_requester, fc::optional<string> memo, bool broadcast);

      variant                           info();
      /** Returns info such as client version, git version of graphene/fc, version of boost, openssl.
       * @returns compile time info and client and dependencies versions
       */
      variant_object                    about() const;
      optional<signed_block_with_info>    get_block( uint32_t num )const;
      optional<signed_block_with_info>    get_block_by_id( block_id_type block_id )const;

      /** Returns the number of accounts registered on the blockchain
       * @returns the number of registered accounts
       */
      uint64_t                          get_account_count()const;

      /**
      * @brief get_data_transaction_product_costs
      * @param start
      * @param end
      * @return the number of the data transaction product costs during this time
      */
      uint64_t                          get_data_transaction_product_costs(fc::time_point_sec start, fc::time_point_sec end) const;

      /**
      * @brief get_data_transaction_total_count
      * @param start
      * @param end
      * @return the number of the data transaction count during this time
      */      
      uint64_t                          get_data_transaction_total_count(fc::time_point_sec start, fc::time_point_sec end) const; 

      /**
      * @brief get_merchants_total_count
      * @return the total count of merchants
      */
      uint64_t                          get_merchants_total_count() const;

      /**
      * @brief get_data_transaction_commission
      * @param start
      * @param end
      * @return the number of the data transaction commission during this time
      */         
      uint64_t                          get_data_transaction_commission(fc::time_point_sec start, fc::time_point_sec end) const;

      /**
      * @brief get_data_transaction_pay_fee
      * @param start
      * @param end
      * @return the number of the data transaction pay fee during this time
      */        
      uint64_t                          get_data_transaction_pay_fee(fc::time_point_sec start, fc::time_point_sec end) const;

      /**
      * @brief get_data_transaction_product_costs_by_requester
      * @param requester
      * @param start
      * @param end
      * @return the number of the data transaction product costs of the requester during this time
      */       
      uint64_t                          get_data_transaction_product_costs_by_requester(string requester, fc::time_point_sec start, fc::time_point_sec end) const;

      /**
      * @brief get_data_transaction_total_count_by_requester
      * @param requester
      * @param start
      * @param end
      * @return the number of the data transaction product count of the requester during this time
      */      
      uint64_t                          get_data_transaction_total_count_by_requester(string requester, fc::time_point_sec start, fc::time_point_sec end) const; 

      /**
      * @brief get_data_transaction_pay_fees_by_requester
      * @param requester
      * @param start
      * @param end
      * @return the number of the data transaction pay fees of the requester during this time
      */      
      uint64_t                          get_data_transaction_pay_fees_by_requester(string requester, fc::time_point_sec start, fc::time_point_sec end) const;

      /**
      * @brief get_data_transaction_pay_fees_by_requester
      * @param product_id
      * @param start
      * @param end
      * @return the number of the data transaction product costs of the product during this time
      */   
      uint64_t                          get_data_transaction_product_costs_by_product_id(string product_id, fc::time_point_sec start, fc::time_point_sec end) const;
      
      /**
      * @brief get_data_transaction_total_count_by_product_id
      * @param product_id
      * @param start
      * @param end
      * @return the number of the data transaction total count of the product during this time
      */ 
      uint64_t                          get_data_transaction_total_count_by_product_id(string product_id, fc::time_point_sec start, fc::time_point_sec end) const;

      /** Lists all accounts controlled by this wallet.
       * This returns a list of the full account objects for all accounts whose private keys 
       * we possess.
       * @returns a list of account objects
       */
      vector<account_object>            list_my_accounts();
      /** Lists all accounts registered in the blockchain.
       * This returns a list of all account names and their account ids, sorted by account name.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all accounts,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last account name returned as the \c lowerbound for the next \c list_accounts() call.
       *
       * @param lowerbound the name of the first account to return.  If the named account does not exist, 
       *                   the list will start at the account that comes after \c lowerbound
       * @param limit the maximum number of accounts to return (max: 1000)
       * @returns a list of accounts mapping account names to account ids
       */
      map<string,account_id_type>       list_accounts(const string& lowerbound, uint32_t limit);
      /** List the balances of an account.
       * Each account can have multiple balances, one for each type of asset owned by that 
       * account.  The returned list will only contain assets for which the account has a
       * nonzero balance
       * @param id the name or id of the account whose balances you want
       * @returns a list of the given account's balances
       */
      vector<asset>                     list_account_balances(const string& id);

      /** List the lock balances of an account.
       * Each account can have multiple lock balances, one for each type of asset owned by that 
       * account.  The returned list will only contain assets for which the account has a
       * nonzero balance
       * @param account_id_or_name the name or id of the account whose lock balances you want
       * @returns a list of the given account's lock balances(this time, lock balance type is only "GXS")
       */
      vector<asset>                     list_account_lock_balances(const string& account_id_or_name);

      /** Lists all assets registered on the blockchain.
       * 
       * To list all assets, pass the empty string \c "" for the lowerbound to start
       * at the beginning of the list, and iterate as necessary.
       *
       * @param lowerbound  the symbol of the first asset to include in the list.
       * @param limit the maximum number of assets to return (max: 100)
       * @returns the list of asset objects, ordered by symbol
       */
      vector<asset_object>              list_assets(const string& lowerbound, uint32_t limit)const;
      
      /** Returns the most recent operations on the named account.
       *
       * This returns a list of operation history objects, which describe activity on the account.
       *
       * @param name the name or id of the account
       * @param limit the number of entries to return (starting from the most recent)
       * @returns a list of \c operation_history_objects
       */
      vector<operation_detail>  get_account_history(string name, int limit)const;

      /**
       * Get irreversible operations relevant to the specified account filtering by operation type, with transaction id
       *
       * @param name the name or id of the account, whose history shoulde be queried
       * @param operation_types The IDs of the operation we want to get operations in the account( 0 = transfer , 1 = limit order create, ...)
       * @param start the sequence number where to start looping back throw the history
       * @param limit the max number of entries to return (from start number)
       * @returns irreversible_account_history_detail
       */
      irreversible_account_history_detail get_irreversible_account_history(string name, vector<uint32_t> operation_types, uint32_t start, int limit) const;

      /** Returns the most recent operations on the named account.
       *
       * This returns a list of operation history objects, which describe activity on the account.
       *
       * @param account_name_or_id the name or id of the account
       * @param operation_indexs the type of operations
       * @param start the start place of the operation_history_objects
       * @param limit the number of entries to return (starting from the most recent)
       * @returns account_history_operation_detail
       */
      account_history_operation_detail get_account_history_by_operations(string account_name_or_id, vector<uint32_t> operation_indexs, uint32_t start, int limit)const;
      
      /** Returns the relative operations on the named account from start number.
       *
       * @param name the name or id of the account
       * @param stop Sequence number of earliest operation.
       * @param limit the number of entries to return
       * @param start  the sequence number where to start looping back throw the history
       * @returns a list of \c operation_history_objects
       */
      vector<operation_detail>  get_relative_account_history(string name, uint32_t stop, int limit, uint32_t start) const;


      vector<bucket_object>             get_market_history(string symbol, string symbol2, uint32_t bucket, fc::time_point_sec start, fc::time_point_sec end)const;
      vector<limit_order_object>        get_limit_orders(string a, string b, uint32_t limit)const;
      vector<call_order_object>         get_call_orders(string a, uint32_t limit)const;
      vector<force_settlement_object>   get_settle_orders(string a, uint32_t limit)const;
      
      /** Returns the block chain's slowly-changing settings.
       * This object contains all of the properties of the blockchain that are fixed
       * or that change only once per maintenance interval (daily) such as the
       * current list of witnesses, committee_members, block interval, etc.
       * @see \c get_dynamic_global_properties() for frequently changing properties
       * @returns the global properties
       */
      global_property_object            get_global_properties() const;

      data_transaction_commission_percent_t get_commission_percent() const;

      /** Returns the block chain's rapidly-changing properties.
       * The returned object contains information that changes every block interval
       * such as the head block number, the next witness, etc.
       * @see \c get_global_properties() for less-frequently changing properties
       * @returns the dynamic global properties
       */
      dynamic_global_property_object    get_dynamic_global_properties() const;

      /** Returns information about the given account.
       *
       * @param account_name_or_id the name or id of the account to provide information about
       * @returns the public account data stored in the blockchain
       */
      account_object                    get_account(string account_name_or_id) const;

      /** Returns information about the given asset.
       * @param asset_name_or_id the symbol or id of the asset in question
       * @returns the information about the asset stored in the block chain
       */
      asset_object                      get_asset(string asset_name_or_id) const;

      /** Returns the BitAsset-specific data for a given asset.
       * Market-issued assets's behavior are determined both by their "BitAsset Data" and
       * their basic asset data, as returned by \c get_asset().
       * @param asset_name_or_id the symbol or id of the BitAsset in question
       * @returns the BitAsset-specific data for this asset
       */
      asset_bitasset_data_object        get_bitasset_data(string asset_name_or_id)const;

      /** Lookup the id of a named account.
       * @param account_name_or_id the name of the account to look up
       * @returns the id of the named account
       */
      account_id_type                   get_account_id(string account_name_or_id) const;

      /**
       * Lookup the id of a named asset.
       * @param asset_name_or_id the symbol of an asset to look up
       * @returns the id of the given asset
       */
      asset_id_type                     get_asset_id(string asset_name_or_id) const;

      /**
       * Returns the blockchain object corresponding to the given id.
       *
       * This generic function can be used to retrieve any object from the blockchain
       * that is assigned an ID.  Certain types of objects have specialized convenience 
       * functions to return their objects -- e.g., assets have \c get_asset(), accounts 
       * have \c get_account(), but this function will work for any object.
       *
       * @param id the id of the object to return
       * @returns the requested object
       */
      variant                           get_object(object_id_type id) const;

      /** Returns the current wallet filename.  
       *
       * This is the filename that will be used when automatically saving the wallet.
       *
       * @see set_wallet_filename()
       * @return the wallet filename
       */
      string                            get_wallet_filename() const;

      /**
       * Get the WIF private key corresponding to a public key.  The
       * private key must already be in the wallet.
       */
      string                            get_private_key( public_key_type pubkey )const;

      /**
       * @ingroup Transaction Builder API
       */
      transaction_handle_type begin_builder_transaction();
      /**
       * @ingroup Transaction Builder API
       */
      void add_operation_to_builder_transaction(transaction_handle_type transaction_handle, const operation& op);
      /**
       * @ingroup Transaction Builder API
       */
      void replace_operation_in_builder_transaction(transaction_handle_type handle,
                                                    unsigned operation_index,
                                                    const operation& new_op);
      /**
       * @ingroup Transaction Builder API
       */
      asset set_fees_on_builder_transaction(transaction_handle_type handle, string fee_asset = GRAPHENE_SYMBOL);
      /**
       * @ingroup Transaction Builder API
       */
      transaction preview_builder_transaction(transaction_handle_type handle);
      /**
       * @ingroup Transaction Builder API
       */
      signed_transaction sign_builder_transaction(transaction_handle_type transaction_handle, bool broadcast = true);

      /** Broadcast signed transaction
       * @param tx signed transaction
       * @returns the transaction ID along with the signed transaction.
       */
      pair<transaction_id_type,signed_transaction> broadcast_transaction(signed_transaction tx);

      /**
       * @ingroup Transaction Builder API
       */
      signed_transaction propose_builder_transaction(
          transaction_handle_type handle,
          time_point_sec expiration = time_point::now() + fc::minutes(1),
          uint32_t review_period_seconds = 0,
          bool broadcast = true
         );

      signed_transaction propose_builder_transaction2(
         transaction_handle_type handle,
         string account_name_or_id,
         time_point_sec expiration = time_point::now() + fc::minutes(1),
         uint32_t review_period_seconds = 0,
         bool broadcast = true
        );

      /**
       * @ingroup Transaction Builder API
       */
      void remove_builder_transaction(transaction_handle_type handle);

      /** Checks whether the wallet has just been created and has not yet had a password set.
       *
       * Calling \c set_password will transition the wallet to the locked state.
       * @return true if the wallet is new
       * @ingroup Wallet Management
       */
      bool    is_new()const;
      
      /** Checks whether the wallet is locked (is unable to use its private keys).  
       *
       * This state can be changed by calling \c lock() or \c unlock().
       * @return true if the wallet is locked
       * @ingroup Wallet Management
       */
      bool    is_locked()const;
      
      /** Locks the wallet immediately.
       * @ingroup Wallet Management
       */
      void    lock();
      
      /** Unlocks the wallet.  
       *
       * The wallet remain unlocked until the \c lock is called
       * or the program exits.
       * @param password the password previously set with \c set_password()
       * @ingroup Wallet Management
       */
      void    unlock(string password);
      
      /** Sets a new password on the wallet.
       *
       * The wallet must be either 'new' or 'unlocked' to
       * execute this command.
       * @ingroup Wallet Management
       */
      void    set_password(string password);

      /** Dumps all private keys owned by the wallet.
       *
       * The keys are printed in WIF format.  You can import these keys into another wallet
       * using \c import_key()
       * @returns a map containing the private keys, indexed by their public key 
       */
      map<public_key_type, string> dump_private_keys();

      /** Returns a list of all commands supported by the wallet API.
       *
       * This lists each command, along with its arguments and return types.
       * For more detailed help on a single command, use \c get_help()
       *
       * @returns a multi-line string suitable for displaying on a terminal
       */
      string  help()const;

      /** Returns detailed help on a single API command.
       * @param method the name of the API command you want help with
       * @returns a multi-line string suitable for displaying on a terminal
       */
      string  gethelp(const string& method)const;

      /** Loads a specified Graphene wallet.
       *
       * The current wallet is closed before the new wallet is loaded.
       *
       * @warning This does not change the filename that will be used for future
       * wallet writes, so this may cause you to overwrite your original
       * wallet unless you also call \c set_wallet_filename()
       *
       * @param wallet_filename the filename of the wallet JSON file to load.
       *                        If \c wallet_filename is empty, it reloads the
       *                        existing wallet file
       * @returns true if the specified wallet is loaded
       */
      bool    load_wallet_file(string wallet_filename = "");

      /** Saves the current wallet to the given filename.
       * 
       * @warning This does not change the wallet filename that will be used for future
       * writes, so think of this function as 'Save a Copy As...' instead of
       * 'Save As...'.  Use \c set_wallet_filename() to make the filename
       * persist.
       * @param wallet_filename the filename of the new wallet JSON file to create
       *                        or overwrite.  If \c wallet_filename is empty,
       *                        save to the current filename.
       */
      void    save_wallet_file(string wallet_filename = "");

      /** Sets the wallet filename used for future writes.
       *
       * This does not trigger a save, it only changes the default filename
       * that will be used the next time a save is triggered.
       *
       * @param wallet_filename the new filename to use for future saves
       */
      void    set_wallet_filename(string wallet_filename);

      /** Suggests a safe brain key to use for creating your account.
       * \c create_account_with_brain_key() requires you to specify a 'brain key',
       * a long passphrase that provides enough entropy to generate cyrptographic
       * keys.  This function will suggest a suitably random string that should
       * be easy to write down (and, with effort, memorize).
       * @returns a suggested brain_key
       */
      brain_key_info suggest_brain_key()const;

     /**
      * Derive any number of *possible* owner keys from a given brain key.
      *
      * NOTE: These keys may or may not match with the owner keys of any account.
      * This function is merely intended to assist with account or key recovery.
      *
      * @see suggest_brain_key()
      *
      * @param brain_key    Brain key
      * @param number_of_desired_keys  Number of desired keys
      * @return A list of keys that are deterministically derived from the brainkey
      */
     vector<brain_key_info> derive_owner_keys_from_brain_key(string brain_key, int number_of_desired_keys = 1) const;

     /**
      * Determine whether a textual representation of a public key
      * (in Base-58 format) is *currently* linked
      * to any *registered* (i.e. non-stealth) account on the blockchain
      * @param public_key Public key
      * @return Whether a public key is known
      */
     bool is_public_key_registered(string public_key) const;

     /**
      * Determine whether an account_name is registered on the blockchain
      * @param name account_name
      * @return true if account_name is registered
      */
      bool is_account_registered(string name) const;

      /** Converts a signed_transaction in JSON form to its binary representation.
       * @param tx the transaction to serialize
       * @returns the binary form of the transaction.  It will not be hex encoded, 
       *          this returns a raw string that may have null characters embedded 
       *          in it
       */
      string serialize_transaction(signed_transaction tx) const;

      /** Converts a proxy_transfer_params in JSON form to its binary representation.
       * @param param the proxy_t ransfer_params to serialize
       * @returns the binary form of the transaction.  It will not be hex encoded,
       *          this returns a raw string that may have null characters embedded
       *          in it
       */
      string serialize_proxy_transfer_params(proxy_transfer_params param) const;

      /** Imports the private key for an existing account.
       *
       * The private key must match either an owner key or an active key for the
       * named account.  
       *
       * @see dump_private_keys()
       *
       * @param account_name_or_id the account owning the key
       * @param wif_key the private key in WIF format
       * @returns true if the key was imported
       */
      bool import_key(string account_name_or_id, string wif_key);

      map<string, bool> import_accounts( string filename, string password );

      bool import_account_keys( string filename, string password, string src_account_name, string dest_account_name );

      /**
       * This call will construct transaction(s) that will claim all balances controled
       * by wif_keys and deposit them into the given account.
       */
      vector< signed_transaction > import_balance( string account_name_or_id, const vector<string>& wif_keys, bool broadcast );

      /** Transforms a brain key to reduce the chance of errors when re-entering the key from memory.
       *
       * This takes a user-supplied brain key and normalizes it into the form used
       * for generating private keys.  In particular, this upper-cases all ASCII characters
       * and collapses multiple spaces into one.
       * @param s the brain key as supplied by the user
       * @returns the brain key in its normalized form
       */
      string normalize_brain_key(string s) const;

      /** Registers a third party's account on the blockckain.
       *
       * This function is used to register an account for which you do not own the private keys.
       * When acting as a registrar, an end user will generate their own private keys and send
       * you the public keys.  The registrar will use this function to register the account
       * on behalf of the end user.
       *
       * @see create_account_with_brain_key()
       *
       * @param name the name of the account, must be unique on the blockchain.  Shorter names
       *             are more expensive to register; the rules are still in flux, but in general
       *             names of more than 8 characters with at least one digit will be cheap.
       * @param owner the owner key for the new account
       * @param active the active key for the new account
       * @param registrar_account the account which will pay the fee to register the user
       * @param referrer_account the account who is acting as a referrer, and may receive a
       *                         portion of the user's transaction fees.  This can be the
       *                         same as the registrar_account if there is no referrer.
       * @param referrer_percent the percentage (0 - 100) of the new user's transaction fees
       *                         not claimed by the blockchain that will be distributed to the
       *                         referrer; the rest will be sent to the registrar.  Will be
       *                         multiplied by GRAPHENE_1_PERCENT when constructing the transaction.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering the account
       */
      signed_transaction register_account(string name,
                                          public_key_type owner,
                                          public_key_type active,
                                          string  registrar_account,
                                          string  referrer_account,
                                          uint32_t referrer_percent,
                                          bool broadcast = false);

      /** Registers a third party's account on the blockckain.
       *
       * This function is used to register an account for which you do not own the private keys.
       * When acting as a registrar, an end user will generate their own private keys and send
       * you the public keys.  The registrar will use this function to register the account
       * on behalf of the end user.
       *
       * @see create_account_with_brain_key()
       *
       * @param name the name of the account, must be unique on the blockchain.  Shorter names
       *             are more expensive to register; the rules are still in flux, but in general
       *             names of more than 8 characters with at least one digit will be cheap.
       * @param owner the owner key for the new account
       * @param active the active key for the new account
       * @param memo the memo key for the new account
       * @param registrar_account the account which will pay the fee to register the user
       * @param referrer_account the account who is acting as a referrer, and may receive a
       *                         portion of the user's transaction fees.  This can be the
       *                         same as the registrar_account if there is no referrer.
       * @param referrer_percent the percentage (0 - 100) of the new user's transaction fees
       *                         not claimed by the blockchain that will be distributed to the
       *                         referrer; the rest will be sent to the registrar.  Will be
       *                         multiplied by GRAPHENE_1_PERCENT when constructing the transaction.
       * @param asset_symbol the symbol or id of the fee.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering the account
       */
      signed_transaction register_account2(string name,
                                          public_key_type owner,
                                          public_key_type active,
                                          public_key_type memo,
                                          string  registrar_account,
                                          string  referrer_account,
                                          uint32_t referrer_percent,
                                          string asset_symbol,
                                          bool broadcast = false);
      /**
       * Upgrades an account to prime status.
       * This makes the account holder a 'lifetime member'.
       *
       * @todo there is no option for annual membership
       * @param name the name or id of the account to upgrade
       * @param asset_symbol the symbol of the fee asset.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction upgrading the account
       */
      signed_transaction upgrade_account(string name, string asset_symbol, bool broadcast);

      /**
       * @brief upgrade_data_transaction_member
       * @param name
       * @param auth_referrer
       * @param broadcast
       * @return
       */
      signed_transaction upgrade_data_transaction_member(string name, string auth_referrer, bool broadcast);

      /**
       * @brief upgrade_account_merchant
       * @param name
       * @param auth_referrer
       * @param broadcast
       * @return
       */
      // 升级账户为商户, 需要发起理事会提案
      signed_transaction upgrade_account_merchant(string name,string auth_referrer, bool broadcast);

      /**
       * @brief upgrade_account_datasource
       * @param name
       * @param auth_referrer
       * @param broadcast
       * @return
       */
       // 升级账户为数据源, 需要发起理事会提案
      signed_transaction upgrade_account_datasource(string name,string auth_referrer, bool broadcast);

      /**
       * @brief get_data_market_category
       * @param category_id
       * @return
       */
      data_market_category_object get_data_market_category(string category_id);
      /**
       * @brief get_free_data_product
       * @param product_id
       * @return
       */
      free_data_product_object get_free_data_product(string product_id);
      /**
       * @brief get_league_data_product
       * @param product_id
       * @return
       */
      league_data_product_object get_league_data_product(string product_id);
      /**
       * @brief get_league
       * @param league_id
       * @return
       */
      league_object get_league(string league_id);

      /**
       * @brief list_data_market_categories
       * @param market_type
       * @return
       */
      vector<data_market_category_object> list_data_market_categories(uint32_t market_type);
      /**
       * @brief list_free_data_products
       * @param data_market_category_id
       * @param offset
       * @param limit
       * @param order_by
       * @param keyword
       * @param show_all
       * @return
       */
      free_data_product_search_results_object list_free_data_products(string data_market_category_id,uint32_t offset,uint32_t limit,string order_by,string keyword,bool show_all = false) const;

      /**
       * @brief list_league_data_products
       * @param data_market_category_id
       * @param offset
       * @param limit
       * @param order_by
       * @param keyword
       * @param show_all
       * @return
       */
      league_data_product_search_results_object list_league_data_products(string data_market_category_id,uint32_t offset,uint32_t limit,string order_by,string keyword,bool show_all = false) const;

      /**
       * @brief list_leagues
       * @param data_market_category_id
       * @param offset
       * @param limit
       * @param order_by
       * @param keyword
       * @param show_all
       * @return
       */
      league_search_results_object list_leagues(string data_market_category_id,uint32_t offset,uint32_t limit,string order_by,string keyword,bool show_all = false) const;

      /**
       * @brief list data_transactions requested by requester
       * @param requester
       * @param limit
       * @return
       */
      data_transaction_search_results_object list_data_transactions_by_requester(string requester, uint32_t limit) const;

       /**
       * @brief list second_hand datasource
       * @param start_date_time
       * @param end_date_time
       * @param limit
       * @return
       */
      map<account_id_type, uint64_t> list_second_hand_datasources(time_point_sec start_date_time, time_point_sec end_date_time, uint32_t limit) const;

       /**
       * @brief list the number of the datasource account who sold second-hand datacource
       * @param start_date_time
       * @param end_date_time
       * @param datasource_account
       * @return
       */
       uint32_t list_total_second_hand_transaction_counts_by_datasource(fc::time_point_sec start_date_time, fc::time_point_sec end_date_time, const string& datasource_account) const;

      
      /**
       * @brief get data_transaction_object by request_id
       * @param request_id
       * @return
       */
      optional<data_transaction_object> get_data_transaction_by_request_id(string request_id) const;

      /** Creates a new account and registers it on the blockchain.
       *
       * @todo why no referrer_percent here?
       *
       * @see suggest_brain_key()
       * @see register_account()
       *
       * @param brain_key the brain key used for generating the account's private keys
       * @param account_name the name of the account, must be unique on the blockchain.  Shorter names
       *                     are more expensive to register; the rules are still in flux, but in general
       *                     names of more than 8 characters with at least one digit will be cheap.
       * @param registrar_account the account which will pay the fee to register the user
       * @param referrer_account the account who is acting as a referrer, and may receive a
       *                         portion of the user's transaction fees.  This can be the
       *                         same as the registrar_account if there is no referrer.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering the account
       */
      signed_transaction create_account_with_brain_key(string brain_key,
                                                       string account_name,
                                                       string registrar_account,
                                                       string referrer_account,
                                                       bool broadcast = false);

      /** Transfer an amount from one account to another.
       * @param from the name or id of the account sending the funds
       * @param to the name or id of the account receiving the funds
       * @param amount the amount to send (in nominal units -- to send half of a BTS, specify 0.5)
       * @param asset_symbol the symbol or id of the asset to send
       * @param memo a memo to attach to the transaction.  The memo will be encrypted in the 
       *             transaction and readable for the receiver.  There is no length limit
       *             other than the limit imposed by maximum transaction size, but transaction
       *             increase with transaction size
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction transferring funds
       */
      signed_transaction transfer(string from,
                                  string to,
                                  string amount,
                                  string asset_symbol,
                                  string memo,
                                  bool broadcast = false);

      /**
       *  This method works just like transfer, except it always broadcasts and
       *  returns the transaction ID along with the signed transaction.
       */
      pair<transaction_id_type,signed_transaction> transfer2(string from,
                                                             string to,
                                                             string amount,
                                                             string asset_symbol,
                                                             string memo ) {
         auto trx = transfer( from, to, amount, asset_symbol, memo, true );
         return std::make_pair(trx.id(),trx);
      }


      /**
       *  This method is used to convert a JSON transaction to its transactin ID.
       */
      transaction_id_type get_transaction_id( const signed_transaction& trx )const { return trx.id(); }

      /** Sign a memo message.
       *
       * @param from the name or id of signing account; or a public key.
       * @param to the name or id of receiving account; or a public key.
       * @param memo text to sign.
       */
      memo_data sign_memo(string from, string to, string memo);

      /** Read a memo.
       *
       * @param memo JSON-enconded memo.
       * @returns string with decrypted message..
       */
      string read_memo(const memo_data& memo);

      /** These methods are used for stealth transfers */
      ///@{
      /**
       *  This method can be used to set the label for a public key
       *
       *  @note No two keys can have the same label.
       *
       *  @return true if the label was set, otherwise false
       */
      bool                        set_key_label( public_key_type, string label );
      string                      get_key_label( public_key_type )const;

      /**
       *  Generates a new blind account for the given brain key and assigns it the given label.
       */
      public_key_type             create_blind_account( string label, string brain_key  );

      /**
       * @return the total balance of all blinded commitments that can be claimed by the
       * given account key or label
       */
      vector<asset>                get_blind_balances( string key_or_label );
      /** @return all blind accounts */
      map<string,public_key_type> get_blind_accounts()const;
      /** @return all blind accounts for which this wallet has the private key */
      map<string,public_key_type> get_my_blind_accounts()const;
      /** @return the public key associated with the given label */
      public_key_type             get_public_key( string label )const;
      ///@}

      /**
       * @return all blind receipts to/form a particular account
       */
      vector<blind_receipt> blind_history( string key_or_account );

      /**
       *  Given a confirmation receipt, this method will parse it for a blinded balance and confirm
       *  that it exists in the blockchain.  If it exists then it will report the amount received and
       *  who sent it.
       *
       *  @param confirmation_receipt - a base58 encoded stealth confirmation 
       *  @param opt_from - if not empty and the sender is a unknown public key, then the unknown public key will be given the label opt_from
       *  @opt_memo - memo
       */
      blind_receipt receive_blind_transfer( string confirmation_receipt, string opt_from, string opt_memo );

      /**
       *  Transfers a public balance from @from to one or more blinded balances using a
       *  stealth transfer.
       */
      blind_confirmation transfer_to_blind( string from_account_id_or_name, 
                                            string asset_symbol,
                                            /** map from key or label to amount */
                                            vector<pair<string, string>> to_amounts, 
                                            bool broadcast = false );

      /**
       * Transfers funds from a set of blinded balances to a public account balance.
       */
      blind_confirmation transfer_from_blind( 
                                            string from_blind_account_key_or_label,
                                            string to_account_id_or_name, 
                                            string amount,
                                            string asset_symbol,
                                            bool broadcast = false );

      /**
       *  Used to transfer from one set of blinded balances to another
       */
      blind_confirmation blind_transfer( string from_key_or_label,
                                         string to_key_or_label,
                                         string amount,
                                         string symbol,
                                         bool broadcast = false );

      /** Creates a new user-issued asset
       *
       * Many options can be changed later using \c update_asset()
       *
       * Right now this function is difficult to use because you must provide raw JSON data
       * structures for the options objects, and those include prices and asset ids.
       *
       * @param issuer the name or id of the account who will pay the fee and become the 
       *               issuer of the new asset.  This can be updated later
       * @param symbol the ticker symbol of the new asset
       * @param precision the number of digits of precision to the right of the decimal point,
       *                  must be less than or equal to 12
       * @param common asset options required for all new assets.
       *               Note that core_exchange_rate technically needs to store the asset ID of 
       *               this new asset. Since this ID is not known at the time this operation is 
       *               created, create this price as though the new asset has instance ID 1, and
       *               the chain will overwrite it with the new asset's ID.
       * @param fee_asset_symbol the symbol of the fee asset.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction creating a new asset
       */
      signed_transaction create_asset(string issuer,
                                      string symbol,
                                      uint8_t precision,
                                      asset_options common,
                                      string fee_asset_symbol,
                                      bool broadcast = false);

      /** Issue new shares of an asset.
       *
       * @param to_account the name or id of the account to receive the new shares
       * @param amount the amount to issue, in nominal units
       * @param symbol the ticker symbol of the asset to issue
       * @param memo a memo to include in the transaction, readable by the recipient
       * @param fee_asset_symbol the symbol of the fee asset.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction issuing the new shares
       */
      signed_transaction issue_asset(string to_account, string amount,
                                     string symbol,
                                     string memo,
                                     string fee_asset_symbol,
                                     bool broadcast = false);

      /** Update the core options on an asset.
       * There are a number of options which all assets in the network use. These options are 
       * enumerated in the asset_object::asset_options struct. This command is used to update 
       * these options for an existing asset.
       *
       * @note This operation cannot be used to update BitAsset-specific options. For these options,
       * \c update_bitasset() instead.
       *
       * @param symbol the name or id of the asset to update
       * @param new_issuer if changing the asset's issuer, the name or id of the new issuer.
       *                   null if you wish to remain the issuer of the asset
       * @param new_options the new asset_options object, which will entirely replace the existing
       *                    options.
       * @param fee_asset_symbol the symbol of the fee asset.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the asset
       */
      signed_transaction update_asset(string symbol,
                                      optional<string> new_issuer,
                                      asset_options new_options,
                                      string fee_asset_symbol,
                                      bool broadcast = false);

      /** Update the options specific to a BitAsset.
       *
       * BitAssets have some options which are not relevant to other asset types. This operation is used to update those
       * options an an existing BitAsset.
       *
       * @see update_asset()
       *
       * @param symbol the name or id of the asset to update, which must be a market-issued asset
       * @param new_options the new bitasset_options object, which will entirely replace the existing
       *                    options.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the bitasset
       */
      signed_transaction update_bitasset(string symbol,
                                         bitasset_options new_options,
                                         bool broadcast = false);

      /** Update the set of feed-producing accounts for a BitAsset.
       *
       * BitAssets have price feeds selected by taking the median values of recommendations from a set of feed producers.
       * This command is used to specify which accounts may produce feeds for a given BitAsset.
       * @param symbol the name or id of the asset to update
       * @param new_feed_producers a list of account names or ids which are authorized to produce feeds for the asset.
       *                           this list will completely replace the existing list
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the bitasset's feed producers
       */
      signed_transaction update_asset_feed_producers(string symbol,
                                                     flat_set<string> new_feed_producers,
                                                     bool broadcast = false);
      
      /** Publishes a price feed for the named asset.
       *
       * Price feed providers use this command to publish their price feeds for market-issued assets. A price feed is
       * used to tune the market for a particular market-issued asset. For each value in the feed, the median across all
       * committee_member feeds for that asset is calculated and the market for the asset is configured with the median of that
       * value.
       *
       * The feed object in this command contains three prices: a call price limit, a short price limit, and a settlement price.
       * The call limit price is structured as (collateral asset) / (debt asset) and the short limit price is structured
       * as (asset for sale) / (collateral asset). Note that the asset IDs are opposite to eachother, so if we're
       * publishing a feed for USD, the call limit price will be CORE/USD and the short limit price will be USD/CORE. The
       * settlement price may be flipped either direction, as long as it is a ratio between the market-issued asset and
       * its collateral.
       *
       * @param publishing_account the account publishing the price feed
       * @param symbol the name or id of the asset whose feed we're publishing
       * @param feed the price_feed object containing the three prices making up the feed
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction updating the price feed for the given asset
       */
      signed_transaction publish_asset_feed(string publishing_account,
                                            string symbol,
                                            price_feed feed,
                                            bool broadcast = false);

      /** Pay into the fee pool for the given asset.
       *
       * User-issued assets can optionally have a pool of the core asset which is 
       * automatically used to pay transaction fees for any transaction using that
       * asset (using the asset's core exchange rate).
       *
       * This command allows anyone to deposit the core asset into this fee pool.
       *
       * @param from the name or id of the account sending the core asset
       * @param symbol the name or id of the asset whose fee pool you wish to fund
       * @param amount the amount of the core asset to deposit
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction funding the fee pool
       */
      signed_transaction fund_asset_fee_pool(string from,
                                             string symbol,
                                             string amount,
                                             bool broadcast = false);

      /** Burns the given user-issued asset.
       *
       * This command burns the user-issued asset to reduce the amount in circulation.
       * @note you cannot burn market-issued assets.
       * @param from the account containing the asset you wish to burn
       * @param amount the amount to burn, in nominal units
       * @param symbol the name or id of the asset to burn
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction burning the asset
       */
      signed_transaction reserve_asset(string from,
                                    string amount,
                                    string symbol,
                                    bool broadcast = false);

      /** Forces a global settling of the given asset (black swan or prediction markets).
       *
       * In order to use this operation, asset_to_settle must have the global_settle flag set
       *
       * When this operation is executed all balances are converted into the backing asset at the
       * settle_price and all open margin positions are called at the settle price.  If this asset is
       * used as backing for other bitassets, those bitassets will be force settled at their current
       * feed price.
       *
       * @note this operation is used only by the asset issuer, \c settle_asset() may be used by 
       *       any user owning the asset
       *
       * @param symbol the name or id of the asset to force settlement on
       * @param settle_price the price at which to settle
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction settling the named asset
       */
      signed_transaction global_settle_asset(string symbol,
                                             price settle_price,
                                             bool broadcast = false);

      /** Schedules a market-issued asset for automatic settlement.
       *
       * Holders of market-issued assests may request a forced settlement for some amount of their asset. This means that
       * the specified sum will be locked by the chain and held for the settlement period, after which time the chain will
       * choose a margin posision holder and buy the settled asset using the margin's collateral. The price of this sale
       * will be based on the feed price for the market-issued asset being settled. The exact settlement price will be the
       * feed price at the time of settlement with an offset in favor of the margin position, where the offset is a
       * blockchain parameter set in the global_property_object.
       *
       * @param account_to_settle the name or id of the account owning the asset
       * @param amount_to_settle the amount of the named asset to schedule for settlement
       * @param symbol the name or id of the asset to settlement on
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction settling the named asset
       */
      signed_transaction settle_asset(string account_to_settle,
                                      string amount_to_settle,
                                      string symbol,
                                      bool broadcast = false);

      /** Whitelist and blacklist accounts, primarily for transacting in whitelisted assets.
       *
       * Accounts can freely specify opinions about other accounts, in the form of either whitelisting or blacklisting
       * them. This information is used in chain validation only to determine whether an account is authorized to transact
       * in an asset type which enforces a whitelist, but third parties can use this information for other uses as well,
       * as long as it does not conflict with the use of whitelisted assets.
       *
       * An asset which enforces a whitelist specifies a list of accounts to maintain its whitelist, and a list of
       * accounts to maintain its blacklist. In order for a given account A to hold and transact in a whitelisted asset S,
       * A must be whitelisted by at least one of S's whitelist_authorities and blacklisted by none of S's
       * blacklist_authorities. If A receives a balance of S, and is later removed from the whitelist(s) which allowed it
       * to hold S, or added to any blacklist S specifies as authoritative, A's balance of S will be frozen until A's
       * authorization is reinstated.
       *
       * @param authorizing_account the account who is doing the whitelisting
       * @param account_to_list the account being whitelisted
       * @param new_listing_status the new whitelisting status
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction changing the whitelisting status
       */
      signed_transaction whitelist_account(string authorizing_account,
                                           string account_to_list,
                                           account_whitelist_operation::account_listing new_listing_status,
                                           bool broadcast = false);

      /** Creates a committee_member object owned by the given account.
       *
       * An account can have at most one committee_member object.
       *
       * @param owner_account the name or id of the account which is creating the committee_member
       * @param url a URL to include in the committee_member record in the blockchain.  Clients may
       *            display this when showing a list of committee_members.  May be blank.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering a committee_member
       */
      signed_transaction create_committee_member(string owner_account,
                                         string url, 
                                         bool broadcast = false);

      /** Lists all witnesses registered in the blockchain.
       * This returns a list of all account names that own witnesses, and the associated witness id,
       * sorted by name.  This lists witnesses whether they are currently voted in or not.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all witnesss,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last witness name returned as the \c lowerbound for the next \c list_witnesss() call.
       *
       * @param lowerbound the name of the first witness to return.  If the named witness does not exist, 
       *                   the list will start at the witness that comes after \c lowerbound
       * @param limit the maximum number of witnesss to return (max: 1000)
       * @returns a list of witnesss mapping witness names to witness ids
       */
      map<string,witness_id_type>       list_witnesses(const string& lowerbound, uint32_t limit);

      /** Lists all committee_members registered in the blockchain.
       * This returns a list of all account names that own committee_members, and the associated committee_member id,
       * sorted by name.  This lists committee_members whether they are currently voted in or not.
       *
       * Use the \c lowerbound and limit parameters to page through the list.  To retrieve all committee_members,
       * start by setting \c lowerbound to the empty string \c "", and then each iteration, pass
       * the last committee_member name returned as the \c lowerbound for the next \c list_committee_members() call.
       *
       * @param lowerbound the name of the first committee_member to return.  If the named committee_member does not exist, 
       *                   the list will start at the committee_member that comes after \c lowerbound
       * @param limit the maximum number of committee_members to return (max: 1000)
       * @returns a list of committee_members mapping committee_member names to committee_member ids
       */
      map<string, committee_member_id_type>       list_committee_members(const string& lowerbound, uint32_t limit);

      /** Returns information about the given witness.
       * @param owner_account the name or id of the witness account owner, or the id of the witness
       * @returns the information about the witness stored in the block chain
       */
      witness_object get_witness(string owner_account);

      /** Returns information about the given committee_member.
       * @param owner_account the name or id of the committee_member account owner, or the id of the committee_member
       * @returns the information about the committee_member stored in the block chain
       */
      committee_member_object get_committee_member(string owner_account);

      /** Creates a witness object owned by the given account.
       *
       * An account can have at most one witness object.
       *
       * @param owner_account the name or id of the account which is creating the witness
       * @param url a URL to include in the witness record in the blockchain.  Clients may
       *            display this when showing a list of witnesses.  May be blank.
       * @param asset_symbol the symbol or id of the fee.
       * @param broadcast true to broadcast the transaction on the network
       * @returns the signed transaction registering a witness
       */
      signed_transaction create_witness(string owner_account,
                                        string url,
                                        string asset_symbol,
                                        bool broadcast = false);

      /**
       * Update a witness object owned by the given account.
       *
       * @param witness_name The name of the witness's owner account.  Also accepts the ID of the owner account or the ID of the witness.
       * @param url Same as for create_witness.  The empty string makes it remain the same.
       * @param block_signing_key The new block signing public key.  The empty string makes it remain the same.
       * @param asset_symbol the symbol or id of the fee.
       * @param broadcast true if you wish to broadcast the transaction.
       */
      signed_transaction update_witness(string witness_name,
                                        string url,
                                        string block_signing_key,
                                        string asset_symbol,
                                        bool broadcast = false);


      /**
       * Create a worker object.
       *
       * @param owner_account The account which owns the worker and will be paid
       * @param work_begin_date When the work begins
       * @param work_end_date When the work ends
       * @param daily_pay Amount of pay per day (NOT per maint interval)
       * @param name Any text
       * @param url Any text
       * @param worker_settings {"type" : "burn"|"refund"|"vesting", "pay_vesting_period_days" : x}
       * @param broadcast true if you wish to broadcast the transaction.
       */
      signed_transaction create_worker(
         string owner_account,
         time_point_sec work_begin_date,
         time_point_sec work_end_date,
         share_type daily_pay,
         string name,
         string url,
         variant worker_settings,
         bool broadcast = false
         );

      /**
       * Update your votes for a worker
       *
       * @param account The account which will pay the fee and update votes.
       * @param worker_vote_delta {"vote_for" : [...], "vote_against" : [...], "vote_abstain" : [...]}
       * @param broadcast true if you wish to broadcast the transaction.
       */
      signed_transaction update_worker_votes(
         string account,
         worker_vote_delta delta,
         bool broadcast = false
         );

      /**
       * Get information about a vesting balance object.
       *
       * @param account_name An account name, account ID, or vesting balance object ID.
       */
      vector< vesting_balance_object_with_info > get_vesting_balances( string account_name );

      /**
       * Withdraw a vesting balance.
       *
       * @param witness_name The account name of the witness, also accepts account ID or vesting balance ID type.
       * @param amount The amount to withdraw.
       * @param asset_symbol The symbol of the asset to withdraw.
       * @param broadcast true if you wish to broadcast the transaction
       */
      signed_transaction withdraw_vesting(
         string witness_name,
         string amount,
         string asset_symbol,
         bool broadcast = false);

      /** Vote for a given committee_member.
       *
       * An account can publish a list of all committee_memberes they approve of.  This 
       * command allows you to add or remove committee_memberes from this list.
       * Each account's vote is weighted according to the number of shares of the
       * core asset owned by that account at the time the votes are tallied.
       *
       * @note you cannot vote against a committee_member, you can only vote for the committee_member
       *       or not vote for the committee_member.
       *
       * @param voting_account the name or id of the account who is voting with their shares
       * @param committee_member the name or id of the committee_member' owner account
       * @param approve true if you wish to vote in favor of that committee_member, false to 
       *                remove your vote in favor of that committee_member
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote for the given committee_member
       */
      signed_transaction vote_for_committee_member(string voting_account,
                                           string committee_member,
                                           bool approve,
                                           bool broadcast = false);

      /** Vote for a given witness.
       *
       * An account can publish a list of all witnesses they approve of.  This 
       * command allows you to add or remove witnesses from this list.
       * Each account's vote is weighted according to the number of shares of the
       * core asset owned by that account at the time the votes are tallied.
       *
       * @note you cannot vote against a witness, you can only vote for the witness
       *       or not vote for the witness.
       *
       * @param voting_account the name or id of the account who is voting with their shares
       * @param witness the name or id of the witness' owner account
       * @param approve true if you wish to vote in favor of that witness, false to 
       *                remove your vote in favor of that witness
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote for the given witness
       */
      signed_transaction vote_for_witness(string voting_account,
                                          string witness,
                                          bool approve,
                                          bool broadcast = false);

      /** Set the voting proxy for an account.
       *
       * If a user does not wish to take an active part in voting, they can choose
       * to allow another account to vote their stake.
       *
       * Setting a vote proxy does not remove your previous votes from the blockchain,
       * they remain there but are ignored.  If you later null out your vote proxy,
       * your previous votes will take effect again.
       *
       * This setting can be changed at any time.
       *
       * @param account_to_modify the name or id of the account to update
       * @param voting_account the name or id of an account authorized to vote account_to_modify's shares,
       *                       or null to vote your own shares
       *
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote proxy settings
       */
      signed_transaction set_voting_proxy(string account_to_modify,
                                          optional<string> voting_account,
                                          bool broadcast = false);
      
      /** Set your vote for the number of witnesses and committee_members in the system.
       *
       * Each account can voice their opinion on how many committee_members and how many 
       * witnesses there should be in the active committee_member/active witness list.  These
       * are independent of each other.  You must vote your approval of at least as many
       * committee_members or witnesses as you claim there should be (you can't say that there should
       * be 20 committee_members but only vote for 10). 
       *
       * There are maximum values for each set in the blockchain parameters (currently 
       * defaulting to 1001).
       *
       * This setting can be changed at any time.  If your account has a voting proxy
       * set, your preferences will be ignored.
       *
       * @param account_to_modify the name or id of the account to update
       * @param desired_number_of_witnesses
       * @param desired_number_of_committee_members
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed transaction changing your vote proxy settings
       */
      signed_transaction set_desired_witness_and_committee_member_count(string account_to_modify,
                                                                uint16_t desired_number_of_witnesses,
                                                                uint16_t desired_number_of_committee_members,
                                                                bool broadcast = false);

      /** Signs a transaction.
       *
       * Given a fully-formed transaction that is only lacking signatures, this signs
       * the transaction with the necessary keys and optionally broadcasts the transaction
       * @param tx the unsigned transaction
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction sign_transaction(signed_transaction tx, bool broadcast = false);

      /** Returns an uninitialized object representing a given blockchain operation.
       *
       * This returns a default-initialized object of the given type; it can be used 
       * during early development of the wallet when we don't yet have custom commands for
       * creating all of the operations the blockchain supports.  
       *
       * Any operation the blockchain supports can be created using the transaction builder's
       * \c add_operation_to_builder_transaction() , but to do that from the CLI you need to 
       * know what the JSON form of the operation looks like.  This will give you a template
       * you can fill in.  It's better than nothing.
       * 
       * @param operation_type the type of operation to return, must be one of the 
       *                       operations defined in `graphene/chain/operations.hpp`
       *                       (e.g., "global_parameters_update_operation")
       * @return a default-constructed operation of the given type
       */
      operation get_prototype_operation(string operation_type);

      /** Creates a transaction to propose a parameter change.
       *
       * Multiple parameters can be specified if an atomic change is
       * desired.
       *
       * @param proposing_account The account paying the fee to propose the tx
       * @param expiration_time Timestamp specifying when the proposal will either take effect or expire.
       * @param changed_values The values to change; all other chain parameters are filled in with default values
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction propose_parameter_change(
         const string& proposing_account,
         fc::time_point_sec expiration_time,
         const variant_object& changed_values,
         bool broadcast = false);

      /** Creates a transaction to propose a extensions parameter change.
       *
       * Multiple parameters can be specified if an atomic change is
       * desired.
       *
       * @param proposing_account The account paying the fee to propose the tx
       * @param expiration_time Timestamp specifying when the proposal will either take effect or expire
       * @param changed_extensions The values to change
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction propose_gpo_extensions_change(
         const string& proposing_account, 
         fc::time_point_sec expiration_time, 
         const variant_object& changed_extensions, 
         bool broadcast = false);

      /**
       *  Creates a transaction to propose a league update
       * @param proposing_account
       * @param league_id
       * @param new_league_name
       * @param new_brief_desc
       * @param new_members
       * @param new_data_products
       * @param new_prices
       * @param new_category_id
       * @param new_icon
       * @param new_status
       * @param new_pocs_thresholds
       * @param new_fee_bases
       * @param new_pocs_weights
       * @param new_recommend
       * @param broadcast
       */
       signed_transaction propose_league_update(
          const string& proposing_account,
          string league_id, 
          string new_league_name,
          string new_brief_desc, 
          vector<account_id_type> new_members,
          vector <league_data_product_id_type> new_data_products, 
          vector <uint64_t> new_prices,
          string new_category_id, 
          string new_icon,
          uint8_t new_status,
          vector<uint64_t> new_pocs_thresholds,
          vector<uint64_t> new_fee_bases,
          vector<uint64_t> new_pocs_weights,
          bool new_recommend, 
          bool broadcast
          );

      /** Propose a fee change.
       * 
       * @param proposing_account The account paying the fee to propose the tx
       * @param expiration_time Timestamp specifying when the proposal will either take effect or expire.
       * @param changed_values Map of operation type to new fee.  Operations may be specified by name or ID.
       *    The "scale" key changes the scale.  All other operations will maintain current values.
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction propose_fee_change(
         const string& proposing_account,
         fc::time_point_sec expiration_time,
         const variant_object& changed_values,
         bool broadcast = false);

       /**  For data sources that violate the rules, remove the data source permissions
       * @param propose_account
       * @param datasource_account
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction propose_deprecate_datasource(string propose_account, string datasource_account, bool broadcast = false);

       /**  For illegal merchants, remove merchant rights while removing data source permissions
       * @param propose_account
       * @param merchant_account
       * @return the signed version of the transaction
       */
      signed_transaction propose_deprecate_merchant(string propose_account, string merchant_account, bool broadcast = false);

      /** Approve or disapprove a proposal.
       *
       * @param fee_paying_account The account paying the fee for the op.
       * @param proposal_id The proposal to modify.
       * @param delta Members contain approvals to create or remove.  In JSON you can leave empty members undefined.
       * @param broadcast true if you wish to broadcast the transaction
       * @return the signed version of the transaction
       */
      signed_transaction approve_proposal(
         const string& fee_paying_account,
         const string& proposal_id,
         const approval_delta& delta,
         bool broadcast /* = false */
         );
         
      order_book get_order_book( const string& base, const string& quote, unsigned limit = 50);

      /** get pocs_object.
       *
       * @param league_id
       * @param account
       * @param product_id
       * @return pocs_object
       */
      optional<pocs_object> get_pocs_object(league_id_type league_id, string account, object_id_type product_id);

      void dbg_make_uia(string creator, string symbol);
      void dbg_push_blocks( std::string src_filename, uint32_t count );
      void dbg_generate_blocks( std::string debug_wif_key, uint32_t count );
      void dbg_stream_json_objects( const std::string& filename );
      void dbg_update_object( fc::variant_object update );

      /** flood_network
       *
       * @param account_prefix
       * @param number_of_transactions
       * @return
       */
      void flood_network(string account_prefix, uint32_t number_of_transactions);
      /** flood_create_account
       *
       * @param account_prefix
       * @param number_of_accounts
       * @return
       */
      void flood_create_account(string account_prefix, uint32_t number_of_accounts);
      /** flood_transfer
       *
       * @param from_account
       * @param account_prefix
       * @param number_of_accounts
       * @param number_of_loop
       * @return
       */
      void flood_transfer(string from_account, string account_prefix, uint32_t number_of_accounts, uint32_t number_of_loop);

      /** transfer_test, Efficient transfer api
       *
       * @param from_account
       * @param to_account
       * @param times
       * @return
       */
      void transfer_test(account_id_type from_account, account_id_type to_account, uint32_t times);

      /** get_hash
       *
       * @param value
       * @return fc::sha256
       */
      fc::sha256 get_hash(const string& value);

      /** verify_transaction_signature 
       * @param trx
       * @param pub_key
       * @return bool
       */
      bool verify_transaction_signature(const signed_transaction& trx, public_key_type pub_key);

      bool verify_proxy_transfer_signature(const proxy_transfer_params& param, public_key_type pub_key);

      /** get tps
       * @return
       */
      void get_tps();

      void network_add_nodes( const vector<string>& nodes );
      vector< variant > network_get_connected_peers();

      /**
       *  Used to transfer from one set of blinded balances to another
       */
      blind_confirmation blind_transfer_help( string from_key_or_label,
                                         string to_key_or_label,
                                         string amount,
                                         string symbol,
                                         bool broadcast = false,
                                         bool to_temp = false );


      std::map<string,std::function<string(fc::variant,const fc::variants&)>> get_result_formatters() const;

      fc::signal<void(bool)> lock_changed;
      std::shared_ptr<detail::wallet_api_impl> my;
      void encrypt_keys();
};

} }

FC_REFLECT( graphene::wallet::key_label, (label)(key) )
FC_REFLECT( graphene::wallet::blind_balance, (amount)(from)(to)(one_time_key)(blinding_factor)(commitment)(used) )
FC_REFLECT( graphene::wallet::blind_confirmation::output, (label)(pub_key)(decrypted_memo)(confirmation)(auth)(confirmation_receipt) )
FC_REFLECT( graphene::wallet::blind_confirmation, (trx)(outputs) )

FC_REFLECT( graphene::wallet::plain_keys, (keys)(checksum) )

FC_REFLECT( graphene::wallet::wallet_data,
            (chain_id)
            (my_accounts)
            (cipher_keys)
            (extra_keys)
            (pending_account_registrations)(pending_witness_registrations)
            (labeled_keys)
            (blind_receipts)
            (ws_server)
            (ws_user)
            (ws_password)
          )

FC_REFLECT( graphene::wallet::brain_key_info,
            (brain_priv_key)
            (wif_priv_key)
            (pub_key)
          )

FC_REFLECT( graphene::wallet::exported_account_keys, (account_name)(encrypted_private_keys)(public_keys) )

FC_REFLECT( graphene::wallet::exported_keys, (password_checksum)(account_keys) )

FC_REFLECT( graphene::wallet::blind_receipt,
            (date)(from_key)(from_label)(to_key)(to_label)(amount)(memo)(control_authority)(data)(used)(conf) )

FC_REFLECT( graphene::wallet::approval_delta,
   (active_approvals_to_add)
   (active_approvals_to_remove)
   (owner_approvals_to_add)
   (owner_approvals_to_remove)
   (key_approvals_to_add)
   (key_approvals_to_remove)
)

FC_REFLECT( graphene::wallet::worker_vote_delta,
   (vote_for)
   (vote_against)
   (vote_abstain)
)

FC_REFLECT_DERIVED( graphene::wallet::vesting_balance_object_with_info, (graphene::chain::vesting_balance_object),
   (allowed_withdraw)(allowed_withdraw_time) )

FC_REFLECT( graphene::wallet::operation_detail, 
            (memo)(description)(op) )

FC_REFLECT( graphene::wallet::operation_detail_ex, 
            (memo)(description)(op)(transaction_id) )

FC_REFLECT( graphene::wallet::account_history_operation_detail,
            (total_without_operations)(details))

FC_REFLECT( graphene::wallet::irreversible_account_history_detail,
            (next_start_sequence)(result_count)(details))

FC_API( graphene::wallet::wallet_api,
        (help)
        (gethelp)
        (info)
        (about)
        (begin_builder_transaction)
        (add_operation_to_builder_transaction)
        (replace_operation_in_builder_transaction)
        (set_fees_on_builder_transaction)
        (preview_builder_transaction)
        (sign_builder_transaction)
        (broadcast_transaction)
        (propose_builder_transaction)
        (propose_builder_transaction2)
        (remove_builder_transaction)
        (is_new)
        (is_locked)
        (lock)(unlock)(set_password)
        (dump_private_keys)
        (list_my_accounts)
        (list_accounts)
        (list_account_balances)
        (list_account_lock_balances)
        (list_assets)
        (import_key)
        (import_accounts)
        (import_account_keys)
        (import_balance)
        (suggest_brain_key)
        (derive_owner_keys_from_brain_key)
        (register_account)
        (register_account2)
        (upgrade_account)
        (create_account_with_brain_key)
        (transfer)
        (transfer2)
        (get_transaction_id)
        (issue_asset)
        (create_asset)
        (update_asset)
        (update_bitasset)
        (update_asset_feed_producers)
        (publish_asset_feed)
        (get_asset)
        (get_bitasset_data)
        (fund_asset_fee_pool)
        (reserve_asset)
        (global_settle_asset)
        (settle_asset)
        (whitelist_account)
        (create_committee_member)
        (get_witness)
        (get_committee_member)
        (list_witnesses)
        (list_committee_members)
        (create_witness)
        (update_witness)
        (create_worker)
        (update_worker_votes)
        (get_vesting_balances)
        (withdraw_vesting)
        (vote_for_committee_member)
        (vote_for_witness)
        (set_voting_proxy)
        (set_desired_witness_and_committee_member_count)
        (get_account)
        (get_account_id)
        (get_block)
        (get_block_by_id)
        (get_commission_percent)
        (get_account_count)
        (get_account_history)
        (get_account_history_by_operations)
        (get_irreversible_account_history)
        (get_relative_account_history)
        (get_data_transaction_product_costs)
        (get_data_transaction_total_count)
        (get_merchants_total_count)
        (get_data_transaction_commission)
        (get_data_transaction_pay_fee)
        (get_data_transaction_product_costs_by_requester)
        (get_data_transaction_total_count_by_requester)
        (get_data_transaction_pay_fees_by_requester)
        (get_data_transaction_product_costs_by_product_id)
        (get_data_transaction_total_count_by_product_id)
        (is_public_key_registered)
        (is_account_registered)
        (get_market_history)
        (get_global_properties)
        (get_dynamic_global_properties)
        (get_object)
        (get_private_key)
        (load_wallet_file)
        (normalize_brain_key)
        (get_limit_orders)
        (get_call_orders)
        (get_settle_orders)
        (save_wallet_file)
        (serialize_transaction)
        (serialize_proxy_transfer_params)
        (sign_transaction)
        (get_prototype_operation)
        (propose_parameter_change)
        (propose_gpo_extensions_change)
        (propose_league_update)
        (propose_fee_change)
        (approve_proposal)
        (dbg_make_uia)
        (dbg_push_blocks)
        (dbg_generate_blocks)
        (dbg_stream_json_objects)
        (dbg_update_object)
        (flood_transfer)
        (transfer_test)
        (get_hash)
        (verify_transaction_signature)
        (verify_proxy_transfer_signature)
        (flood_network)
        (flood_create_account)
        (get_tps)
        (network_add_nodes)
        (network_get_connected_peers)
        (sign_memo)
        (read_memo)
        (set_key_label)
        (get_key_label)
        (get_public_key)
        (get_blind_accounts)
        (get_my_blind_accounts)
        (get_blind_balances)
        (create_blind_account)
        (transfer_to_blind)
        (transfer_from_blind)
        (blind_transfer)
        (blind_history)
        (receive_blind_transfer)
        (get_order_book)
        (create_data_market_category)
        (propose_data_market_category_update)
        (create_free_data_product)
        (propose_free_data_product_update)
        (create_league_data_product)
        (propose_league_data_product_update)
        (propose_deprecate_datasource)
        (propose_deprecate_merchant)
        (create_league)
        (data_transaction_create)
        (data_transaction_update)
        (pay_data_transaction)
        (data_transaction_datasource_upload)
        (data_transaction_datasource_validate_error)
        (data_transaction_complain_datasource)
        (list_data_transaction_complain_requesters)
        (list_data_transaction_complain_datasources)
        (upgrade_account_merchant)
        (upgrade_account_datasource)
        (upgrade_data_transaction_member)
        (get_data_market_category)
        (get_free_data_product)
        (get_league_data_product)
        (get_league)
        (list_data_market_categories)
        (list_free_data_products)
        (list_league_data_products)
        (list_leagues)
        (list_data_transactions_by_requester)
        (list_second_hand_datasources)
        (list_total_second_hand_transaction_counts_by_datasource)
        (get_data_transaction_by_request_id)
        (get_pocs_object)
)
